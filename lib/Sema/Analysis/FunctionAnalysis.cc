#include "Sema/Analysis/FunctionAnalysis.h"

#include <utl/scope_guard.hpp>
#include <utl/stack.hpp>

#include "AST/AST.h"
#include "Common/Base.h"
#include "Common/Ranges.h"
#include "Sema/Analysis/AnalysisContext.h"
#include "Sema/Analysis/ConstantExpressions.h"
#include "Sema/Analysis/Conversion.h"
#include "Sema/Analysis/DTorStack.h"
#include "Sema/Analysis/ExpressionAnalysis.h"
#include "Sema/Analysis/Lifetime.h"
#include "Sema/Analysis/Utility.h"
#include "Sema/Entity.h"
#include "Sema/SemanticIssue.h"
#include "Sema/SemanticIssuesNEW.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;
using enum ValueCategory;
using enum ConversionKind;

static void gatherParentDestructorsImpl(ast::Statement& stmt, auto condition) {
    for (auto* parentScope = cast<ast::Statement*>(stmt.parent());
         condition(parentScope);
         parentScope = dyncast<ast::Statement*>(parentScope->parent()))
    {
        for (auto dtorCall: parentScope->dtorStack() | ranges::views::reverse) {
            stmt.pushDtor(dtorCall);
        }
    }
}

static void gatherParentDestructors(ast::ReturnStatement& stmt) {
    gatherParentDestructorsImpl(stmt, [](auto* parent) {
        return !isa<ast::FunctionDefinition>(parent);
    });
}

static void gatherParentDestructors(ast::JumpStatement& stmt) {
    gatherParentDestructorsImpl(stmt, [](auto* parent) {
        return !isa<ast::LoopStatement>(parent);
    });
}

namespace {

struct FuncBodyContext {
    FuncBodyContext(sema::AnalysisContext& ctx,
                    ast::FunctionDefinition& function):
        ctx(ctx),
        sym(ctx.symbolTable()),
        iss(ctx.issueHandler()),
        currentFunction(function) {}

    void run() { analyze(currentFunction); }

    void analyze(ast::ASTNode&);

    void analyzeImpl(ast::FunctionDefinition&);
    void analyzeImpl(ast::ParameterDeclaration&);
    void analyzeImpl(ast::ThisParameter&);
    void analyzeImpl(ast::StructDefinition&);
    void analyzeImpl(ast::CompoundStatement&);
    void analyzeImpl(ast::VariableDeclaration&);
    void analyzeImpl(ast::ExpressionStatement&);
    void analyzeImpl(ast::ReturnStatement&);
    void analyzeImpl(ast::IfStatement&);
    void analyzeImpl(ast::LoopStatement&);
    void analyzeImpl(ast::JumpStatement&);
    void analyzeImpl(ast::EmptyStatement&) {}
    void analyzeImpl(ast::ASTNode& node) { SC_UNREACHABLE(); }

    ast::Expression* analyzeValue(ast::Expression* expr, DTorStack& dtorStack) {
        return sema::analyzeValueExpr(expr, dtorStack, ctx);
    }

    Type const* analyzeType(ast::Expression* expr) {
        return sema::analyzeTypeExpr(expr, ctx);
    }

    sema::AnalysisContext& ctx;
    SymbolTable& sym;
    IssueHandler& iss;
    ast::FunctionDefinition& currentFunction;
    /// Only needed if return type is not specified
    sema::Type const* deducedReturnType = nullptr;
};

} // namespace

void sema::analyzeFunction(AnalysisContext& ctx, ast::FunctionDefinition* def) {
    ctx.symbolTable().withScopeCurrent(def->function()->parent(), [&] {
        FuncBodyContext funcBodyCtx(ctx, *def);
        funcBodyCtx.run();
    });
}

void FuncBodyContext::analyze(ast::ASTNode& node) {
    visit(node, [this](auto& node) { this->analyzeImpl(node); });
}

void FuncBodyContext::analyzeImpl(ast::FunctionDefinition& fn) {
    if (auto const sk = sym.currentScope().kind(); sk != ScopeKind::Global &&
                                                   sk != ScopeKind::Namespace &&
                                                   sk != ScopeKind::Type)
    {
        /// Function defintion is only allowed in the global scope, at namespace
        /// scope and structure scope.
        ctx.issue<GenericBadStmt>(&fn, GenericBadStmt::InvalidScope);
        sym.declarePoison(std::string(fn.name()), EntityCategory::Value);
        return;
    }
    SC_ASSERT(&fn == &currentFunction,
              "We only analyze one function per context object");
    /// Here the AST node is partially decorated: `entity()` is already set by
    /// `gatherNames()` phase, now we complete the decoration.
    auto* semaFn = fn.function();
    if (ctx.isAnalyzed(semaFn) || ctx.isAnalyzing(semaFn)) {
        /// We don't emit errors here if the function is currently analyzing
        /// because the error should appear at the callsite
        return;
    }
    ctx.beginAnalyzing(semaFn);
    utl::scope_guard guard([&] { ctx.endAnalyzing(semaFn); });
    fn.decorateFunction(semaFn, semaFn->returnType());
    fn.body()->decorateScope(semaFn);
    semaFn->setBinaryVisibility(fn.binaryVisibility());
    /// Maybe try to abstract this later and perform some more checks on main,
    /// but for now we just do this here
    if (semaFn->name() == "main") {
        semaFn->setBinaryVisibility(BinaryVisibility::Export);
    }
    sym.withScopePushed(semaFn, [&] {
        for (auto* param: fn.parameters()) {
            analyze(*param);
        }
    });
    /// The function body compound statement pushes the scope again
    analyze(*fn.body());
    /// If we have deduced a return type in the return statements we set it here
    if (!semaFn->returnType()) {
        if (deducedReturnType) {
            semaFn->setDeducedReturnType(deducedReturnType);
        }
        else {
            semaFn->setDeducedReturnType(sym.Void());
        }
    }
}

void FuncBodyContext::analyzeImpl(ast::ParameterDeclaration& paramDecl) {
    Type const* declaredType =
        currentFunction.function()->argumentType(paramDecl.index());
    if (!declaredType) {
        sym.declarePoison(std::string(paramDecl.name()), EntityCategory::Value);
        return;
    }
    auto* param =
        sym.defineVariable(&paramDecl, declaredType, paramDecl.mutability());
    if (param) {
        paramDecl.decorateVarDecl(param);
    }
}

void FuncBodyContext::analyzeImpl(ast::ThisParameter& thisParam) {
    auto* function = cast<Function*>(currentFunction.entity());
    auto* parentType = dyncast<ObjectType*>(function->parent());
    if (!parentType) {
        return;
    }
    /// We already check the position during instantiation
    auto* param = [&] {
        if (thisParam.isReference()) {
            auto* type = sym.reference({ parentType, thisParam.mutability() });
            return sym.defineVariable("__this", type, Mutability::Const);
        }
        else {
            return sym.defineVariable("__this",
                                      parentType,
                                      thisParam.mutability());
        }
    }();
    if (param) {
        function->setIsMember();
        thisParam.decorateVarDecl(param);
    }
}

void FuncBodyContext::analyzeImpl(ast::StructDefinition& def) {
    /// Function defintion is only allowed in the global scope, at namespace
    /// scope and structure scope.
    sym.declarePoison(std::string(def.name()), EntityCategory::Type);
    ctx.issue<GenericBadStmt>(&def, GenericBadStmt::InvalidScope);
}

void FuncBodyContext::analyzeImpl(ast::CompoundStatement& block) {
    if (!block.isDecorated()) {
        block.decorateScope(sym.addAnonymousScope());
    }
    sym.withScopePushed(block.scope(), [&] {
        /// We make a copy why?
        for (auto* statement: block.statements() | ToSmallVector<>) {
            analyze(*statement);
        }
    });
}

void FuncBodyContext::analyzeImpl(ast::VariableDeclaration& varDecl) {
    SC_ASSERT(!varDecl.isDecorated(),
              "We should not have handled local variables in prepass.");
    if (!varDecl.initExpr() && !varDecl.typeExpr()) {
        sym.declarePoison(std::string(varDecl.name()), EntityCategory::Value);
        ctx.issue<BadVarDecl>(&varDecl, BadVarDecl::CantInferType);
        return;
    }
    auto* initExpr = analyzeValue(varDecl.initExpr(), varDecl.dtorStack());
    auto declType = analyzeType(varDecl.typeExpr());
    auto initType = initExpr ? initExpr->type().get() : nullptr;
    auto type = declType ? declType : initType;
    if (!type) {
        sym.declarePoison(std::string(varDecl.name()), EntityCategory::Value);
        return;
    }
    if (!type->isComplete()) {
        sym.declarePoison(std::string(varDecl.name()), EntityCategory::Value);
        ctx.issue<BadVarDecl>(&varDecl,
                              BadVarDecl::IncompleteType,
                              type,
                              initExpr);
        return;
    }
    if (isa<ReferenceType>(type) && !initExpr) {
        sym.declarePoison(std::string(varDecl.name()), EntityCategory::Value);
        ctx.issue<BadVarDecl>(&varDecl, BadVarDecl::ExpectedRefInit);
        return;
    }
    auto* variable = sym.defineVariable(&varDecl, type, varDecl.mutability());
    if (!variable) {
        return;
    }
    varDecl.decorateVarDecl(variable);
    if (initExpr) {
        convert(Implicit,
                initExpr,
                variable->getQualType(),
                refToLValue(type),
                varDecl.dtorStack(),
                ctx);
        popTopLevelDtor(initExpr, varDecl.dtorStack());
    }
    else {
        auto call = makePseudoConstructorCall(cast<ObjectType const*>(type),
                                              nullptr,
                                              {},
                                              varDecl.dtorStack(),
                                              ctx,
                                              varDecl.sourceRange());
        /// We don't need to declare poison here if there is no matching
        /// constructor because the type is explicitly declared. We will get an
        /// error but we can analyze uses of this variable.
        varDecl.setInitExpr(std::move(call));
    }
    if (variable->isConst() && initExpr) {
        variable->setConstantValue(clone(initExpr->constantValue()));
    }
    cast<ast::Statement*>(varDecl.parent())->pushDtor(variable);
}

void FuncBodyContext::analyzeImpl(ast::ExpressionStatement& es) {
    SC_ASSERT(sym.currentScope().kind() == ScopeKind::Function, "");
    analyzeValue(es.expression(), es.dtorStack());
}

void FuncBodyContext::analyzeImpl(ast::ReturnStatement& rs) {
    SC_ASSERT(sym.currentScope().kind() == ScopeKind::Function, "");
    Type const* returnType = currentFunction.returnType();
    if (!rs.expression() && !isa_or_null<VoidType>(returnType)) {
        ctx.issue<BadReturnStmt>(&rs, BadReturnStmt::NonVoidMustReturnValue);
        return;
    }
    /// We gather parent destructors here because `analyzeExpr()` may add more
    /// constructors and the parent destructors must be lower in the stack
    gatherParentDestructors(rs);
    if (!analyzeValue(rs.expression(), rs.dtorStack())) {
        return;
    }
    if (isa_or_null<VoidType>(returnType)) {
        ctx.issue<BadReturnStmt>(&rs, BadReturnStmt::VoidMustNotReturnValue);
        return;
    }
    if (returnType) {
        convert(Implicit,
                rs.expression(),
                getQualType(returnType),
                refToLValue(returnType),
                rs.dtorStack(),
                ctx);
    }
    else {
        returnType = rs.expression()->type().get();
        if (!deducedReturnType) {
            deducedReturnType = returnType;
        }
        if (deducedReturnType != returnType) {
            /// TODO: Push error here.
            /// For return type deduction all return statements must return the
            /// same type
            SC_UNIMPLEMENTED();
        }
    }
    popTopLevelDtor(rs.expression(), rs.dtorStack());
}

void FuncBodyContext::analyzeImpl(ast::IfStatement& stmt) {
    if (sym.currentScope().kind() != ScopeKind::Function) {
        ctx.issue<GenericBadStmt>(&stmt, GenericBadStmt::InvalidScope);
        return;
    }
    if (analyzeValue(stmt.condition(), stmt.dtorStack())) {
        convert(Implicit,
                stmt.condition(),
                sym.Bool(),
                RValue,
                stmt.dtorStack(),
                ctx);
    }
    analyze(*stmt.thenBlock());
    if (stmt.elseBlock()) {
        analyze(*stmt.elseBlock());
    }
}

void FuncBodyContext::analyzeImpl(ast::LoopStatement& stmt) {
    if (sym.currentScope().kind() != ScopeKind::Function) {
        ctx.issue<GenericBadStmt>(&stmt, GenericBadStmt::InvalidScope);
        return;
    }
    stmt.block()->decorateScope(sym.addAnonymousScope());
    sym.pushScope(stmt.block()->scope());
    if (stmt.varDecl()) {
        analyze(*stmt.varDecl());
    }
    if (analyzeValue(stmt.condition(), stmt.conditionDtorStack())) {
        convert(Implicit,
                stmt.condition(),
                sym.Bool(),
                RValue,
                stmt.conditionDtorStack(),
                ctx);
    }
    if (stmt.increment()) {
        analyzeValue(stmt.increment(), stmt.incrementDtorStack());
    }
    sym.popScope(); /// The block will push its scope again.
    analyze(*stmt.block());
}

void FuncBodyContext::analyzeImpl(ast::JumpStatement& stmt) {
    auto* parent = stmt.parent();
    while (true) {
        if (!parent || isa<ast::FunctionDefinition>(parent)) {
            ctx.issue<GenericBadStmt>(&stmt, GenericBadStmt::InvalidScope);
            return;
        }
        if (isa<ast::LoopStatement>(parent)) {
            break;
        }
        parent = parent->parent();
    }
    gatherParentDestructors(stmt);
}
