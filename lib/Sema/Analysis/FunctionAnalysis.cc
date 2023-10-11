#include "Sema/Analysis/FunctionAnalysis.h"

#include <utl/scope_guard.hpp>
#include <utl/stack.hpp>

#include "AST/AST.h"
#include "Common/Base.h"
#include "Common/Ranges.h"
#include "Sema/Analysis/AnalysisContext.h"
#include "Sema/Analysis/ConstantExpressions.h"
#include "Sema/Analysis/Conversion.h"
#include "Sema/Analysis/ExpressionAnalysis.h"
#include "Sema/Analysis/Utility.h"
#include "Sema/DtorStack.h"
#include "Sema/Entity.h"
#include "Sema/SemaIssues.h"
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
        currentFunction(function),
        semaFn(function.function()) {}

    void run() { analyze(currentFunction); }

    void analyze(ast::ASTNode&);

    void analyzeImpl(ast::FunctionDefinition&);
    void analyzeMainFunction();
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

    ast::Expression* analyzeValue(ast::Expression* expr, DtorStack& dtorStack) {
        return sema::analyzeValueExpr(expr, dtorStack, ctx);
    }

    Type const* analyzeType(ast::Expression* expr) {
        return sema::analyzeTypeExpr(expr, ctx);
    }

    /// Used by return statement case to add a type to return type deduction
    void deduceReturnTypeTo(ast::ReturnStatement const* stmt,
                            sema::Type const* type);

    /// Called by function definition case after analyzing the body
    void setDeducedReturnType();

    /// \Returns `true` if the current function returns by reference. In that
    /// case we don't pop destructor for our return value
    bool returnsRef() const {
        /// For now! If we add slim ref qualifiers with type deduction this
        /// needs to change
        if (!currentFunction.returnType()) {
            return false;
        }
        return isa<ReferenceType>(currentFunction.returnType());
    }

    sema::AnalysisContext& ctx;
    SymbolTable& sym;
    IssueHandler& iss;
    ast::FunctionDefinition& currentFunction;
    sema::Function* semaFn;
    /// Only needed if return type is not specified
    sema::Type const* deducedRetTy = nullptr;
    ast::ReturnStatement const* lastReturn = nullptr;
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

void FuncBodyContext::analyzeImpl(ast::FunctionDefinition& def) {
    if (auto const sk = sym.currentScope().kind(); sk != ScopeKind::Global &&
                                                   sk != ScopeKind::Namespace &&
                                                   sk != ScopeKind::Type)
    {
        /// Function defintion is only allowed in the global scope, at namespace
        /// scope and structure scope.
        ctx.issue<GenericBadStmt>(&def, GenericBadStmt::InvalidScope);
        sym.declarePoison(std::string(def.name()), EntityCategory::Value);
        return;
    }
    SC_ASSERT(&def == &currentFunction,
              "We only analyze one function per context object");
    /// Here the AST node is partially decorated: `entity()` is already set by
    /// `gatherNames()` phase, now we complete the decoration.
    if (ctx.isAnalyzed(semaFn) || ctx.isAnalyzing(semaFn)) {
        /// We don't emit errors here if the function is currently analyzing
        /// because the error should appear at the callsite
        return;
    }
    ctx.beginAnalyzing(semaFn);
    utl::scope_guard guard([&] { ctx.endAnalyzing(semaFn); });
    def.decorateFunction(semaFn, semaFn->returnType());
    def.body()->decorateScope(semaFn);
    semaFn->setBinaryVisibility(def.binaryVisibility());
    sym.withScopePushed(semaFn, [&] {
        for (auto* param: def.parameters()) {
            analyze(*param);
        }
    });
    /// The function body compound statement pushes the scope again
    analyze(*def.body());
    setDeducedReturnType();
    /// We perform the extra checks on main in the end because here we have
    /// deduced the return type
    if (semaFn->name() == "main") {
        analyzeMainFunction();
    }
}

/// Here we perform all checks and transforms on `main` that make it special
void FuncBodyContext::analyzeMainFunction() {
    semaFn->setBinaryVisibility(BinaryVisibility::Export);
    auto* retType = semaFn->returnType();
    /// We might require main to return int at some point, but right now there
    /// are many test cases where main returns bool or double
    if (!retType->hasTrivialLifetime()) {
        ctx.issue<GenericBadStmt>(&currentFunction,
                                  GenericBadStmt::MainMustReturnTrivial);
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
        Type const* type = parentType;
        auto mut = thisParam.mutability();
        if (thisParam.isReference()) {
            type = sym.reference({ parentType, thisParam.mutability() });
            mut = Mutability::Const;
        }
        return sym.addProperty(PropertyKind::This, type, mut, LValue);
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
    /// We need at least one of init expression and type specifier
    if (!varDecl.initExpr() && !varDecl.typeExpr()) {
        sym.declarePoison(std::string(varDecl.name()), EntityCategory::Value);
        ctx.issue<BadVarDecl>(&varDecl, BadVarDecl::CantInferType);
        return;
    }
    auto* initExpr = analyzeValue(varDecl.initExpr(), varDecl.dtorStack());
    auto declType = analyzeType(varDecl.typeExpr());
    auto initType = initExpr ? initExpr->type().get() : nullptr;
    auto type = declType ? declType : initType;
    /// We cannot deduce the type of the variable
    if (!type) {
        sym.declarePoison(std::string(varDecl.name()), EntityCategory::Value);
        return;
    }
    /// The type must be complete, that means no `void` and no dynamic arrays
    if (!type->isComplete()) {
        sym.declarePoison(std::string(varDecl.name()), EntityCategory::Value);
        ctx.issue<BadVarDecl>(&varDecl,
                              BadVarDecl::IncompleteType,
                              type,
                              initExpr);
        return;
    }
    /// Reference variables must be initalized explicitly
    if (isa<ReferenceType>(type) && !initExpr) {
        sym.declarePoison(std::string(varDecl.name()), EntityCategory::Value);
        ctx.issue<BadVarDecl>(&varDecl, BadVarDecl::ExpectedRefInit);
        return;
    }
    /// If the symbol table complains we also return early
    auto* variable = sym.defineVariable(&varDecl, type, varDecl.mutability());
    if (!variable) {
        return;
    }
    varDecl.decorateVarDecl(variable);
    /// If we have an init expression we convert it to the type of the variable.
    /// If the type is derived from the init expression then this is a no-op
    if (varDecl.initExpr()) {
        /// Test `initExpr` because the variable may have an invalid init
        /// expression, in this case `varDecl.initExpr()` is not null but
        /// `initExpr` is.
        if (initExpr) {
            initExpr = convert(Implicit,
                               initExpr,
                               variable->getQualType(),
                               refToLValue(type),
                               varDecl.dtorStack(),
                               ctx);
        }
    }
    /// Otherwise we construct an object of the declared type without arguments
    else {
        auto* objType = cast<ObjectType const*>(type);
        auto constructExpr =
            allocate<ast::ConstructExpr>(objType, varDecl.sourceRange());
        initExpr = varDecl.setInitExpr(std::move(constructExpr));
        initExpr = analyzeValue(initExpr, varDecl.dtorStack());
    }
    /// If our variable is of object type, we pop the last destructor _in the
    /// stack of this declaration_ because it corresponds to the object whose
    /// lifetime this variable shall extend. Then we push the destructor to the
    /// stack of the parent statement.
    if (!isa<ReferenceType>(varDecl.type())) {
        popTopLevelDtor(initExpr, varDecl.dtorStack());
        cast<ast::Statement*>(varDecl.parent())->pushDtor(variable);
    }
    /// Propagate constant value
    if (variable->isConst() && initExpr) {
        variable->setConstantValue(clone(initExpr->constantValue()));
    }
}

void FuncBodyContext::analyzeImpl(ast::ExpressionStatement& es) {
    SC_ASSERT(sym.currentScope().kind() == ScopeKind::Function, "");
    analyzeValue(es.expression(), es.dtorStack());
}

void FuncBodyContext::analyzeImpl(ast::ReturnStatement& rs) {
    SC_ASSERT(sym.currentScope().kind() == ScopeKind::Function, "");
    /// We gather parent destructors here because `analyzeValue()` may add more
    /// constructors and the parent destructors must be lower in the stack
    gatherParentDestructors(rs);
    Type const* returnType = currentFunction.returnType();
    /// "Naked" `return;` case
    if (!rs.expression()) {
        if (!returnType) {
            deduceReturnTypeTo(&rs, sym.Void());
        }
        else if (!isa<VoidType>(returnType)) {
            ctx.issue<BadReturnStmt>(&rs,
                                     BadReturnStmt::NonVoidMustReturnValue);
        }
        /// Else we return `void` as expected
        return;
    }
    /// We return an expression
    if (!analyzeValue(rs.expression(), rs.dtorStack())) {
        return;
    }
    if (isa<VoidType>(returnType)) {
        ctx.issue<BadReturnStmt>(&rs, BadReturnStmt::VoidMustNotReturnValue);
        return;
    }
    if (!returnType) {
        returnType = rs.expression()->type().get();
        deduceReturnTypeTo(&rs, returnType);
    }
    convert(Implicit,
            rs.expression(),
            getQualType(returnType),
            refToLValue(returnType),
            rs.dtorStack(),
            ctx);
    if (!returnsRef()) {
        popTopLevelDtor(rs.expression(), rs.dtorStack());
    }
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

void FuncBodyContext::deduceReturnTypeTo(ast::ReturnStatement const* stmt,
                                         sema::Type const* type) {
    if (!deducedRetTy) {
        deducedRetTy = type;
    }
    if (deducedRetTy != type) {
        ctx.issue<BadReturnTypeDeduction>(stmt, lastReturn);
    }
    lastReturn = stmt;
}

void FuncBodyContext::setDeducedReturnType() {
    if (semaFn->returnType()) {
        return;
    }
    if (!deducedRetTy) {
        semaFn->setDeducedReturnType(sym.Void());
        return;
    }
    if (deducedRetTy != sym.Void() && !deducedRetTy->isComplete()) {
        ctx.issue<BadPassedType>(lastReturn->expression(),
                                 BadPassedType::ReturnDeduced);
    }
    semaFn->setDeducedReturnType(deducedRetTy);
}
