#include "Sema/Analysis/FunctionBodyAnalysis.h"

#include <utl/scope_guard.hpp>
#include <utl/stack.hpp>

#include "AST/AST.h"
#include "Common/Base.h"
#include "Common/Ranges.h"
#include "Sema/Analysis/ConstantExpressions.h"
#include "Sema/Analysis/Conversion.h"
#include "Sema/Analysis/DTorStack.h"
#include "Sema/Analysis/ExpressionAnalysis.h"
#include "Sema/Analysis/Lifetime.h"
#include "Sema/Analysis/Utility.h"
#include "Sema/Context.h"
#include "Sema/Entity.h"
#include "Sema/SemanticIssue.h"
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
    FuncBodyContext(sema::Context& ctx):
        ctx(ctx), sym(ctx.symbolTable()), iss(ctx.issueHandler()) {}

    void analyze(ast::ASTNode&);

    void analyzeImpl(ast::FunctionDefinition&);
    void analyzeImpl(ast::StructDefinition&);
    void analyzeImpl(ast::CompoundStatement&);
    void analyzeImpl(ast::VariableDeclaration&);
    void analyzeImpl(ast::ParameterDeclaration&);
    void analyzeImpl(ast::ThisParameter&);
    void analyzeImpl(ast::ExpressionStatement&);
    void analyzeImpl(ast::ReturnStatement&);
    void analyzeImpl(ast::IfStatement&);
    void analyzeImpl(ast::LoopStatement&);
    void analyzeImpl(ast::JumpStatement&);
    void analyzeImpl(ast::EmptyStatement&) {}
    void analyzeImpl(ast::ASTNode& node) { SC_UNREACHABLE(); }

    ast::Expression* analyzeExpr(ast::Expression* expr, DTorStack& dtorStack) {
        return sema::analyzeExpression(expr, dtorStack, ctx);
    }

    sema::Context& ctx;
    SymbolTable& sym;
    IssueHandler& iss;
    ast::FunctionDefinition* currentFunction = nullptr;
    size_t paramIndex = 0;
};

} // namespace

void sema::analyzeFunctionBodies(
    Context& ctx, std::span<ast::FunctionDefinition* const> functions) {
    FuncBodyContext funcBodyCtx(ctx);
    auto& sym = ctx.symbolTable();
    for (auto* def: functions) {
        sym.makeScopeCurrent(def->function()->parent());
        funcBodyCtx.analyzeImpl(cast<ast::FunctionDefinition&>(*def));
        sym.makeScopeCurrent(nullptr);
    }
}

void FuncBodyContext::analyze(ast::ASTNode& node) {
    visit(node, [this](auto& node) { this->analyzeImpl(node); });
}

void FuncBodyContext::analyzeImpl(ast::FunctionDefinition& fn) {
    if (auto const sk = sym.currentScope().kind(); sk != ScopeKind::Global &&
                                                   sk != ScopeKind::Namespace &&
                                                   sk != ScopeKind::Object)
    {
        /// Function defintion is only allowed in the global scope, at namespace
        /// scope and structure scope.
        iss.push<InvalidDeclaration>(
            &fn,
            InvalidDeclaration::Reason::InvalidInCurrentScope,
            sym.currentScope());
        sym.declarePoison(std::string(fn.name()), EntityCategory::Value);
        return;
    }
    SC_ASSERT(fn.function(),
              "Can't analyze the body if wen don't have a symbol to push this "
              "functions scope.");
    /// Here the AST node is partially decorated: `entity()` is already set by
    /// `gatherNames()` phase, now we complete the decoration.
    auto* function = fn.function();
    fn.decorateFunction(function, function->returnType());
    fn.body()->decorateScope(function);
    function->setBinaryVisibility(fn.binaryVisibility());
    /// Maybe try to abstract this later and perform some more checks on main,
    /// but for now we just do this here
    if (function->name() == "main") {
        function->setBinaryVisibility(BinaryVisibility::Export);
    }
    currentFunction = &fn;
    paramIndex = 0;
    sym.pushScope(function);
    utl::armed_scope_guard popScope = [&] { sym.popScope(); };
    for (auto* param: fn.parameters()) {
        analyze(*param);
    }
    /// The body will push the scope itself again.
    popScope.execute();
    analyze(*fn.body());
}

void FuncBodyContext::analyzeImpl(ast::StructDefinition& s) {
    auto const sk = sym.currentScope().kind();
    if (sk != ScopeKind::Global && sk != ScopeKind::Namespace &&
        sk != ScopeKind::Object)
    {
        /// Function defintion is only allowed in the global scope, at namespace
        /// scope and structure scope.
        iss.push<InvalidDeclaration>(
            &s,
            InvalidDeclaration::Reason::InvalidInCurrentScope,
            sym.currentScope());
        sym.declarePoison(std::string(s.name()), EntityCategory::Type);
    }
}

void FuncBodyContext::analyzeImpl(ast::CompoundStatement& block) {
    if (!block.isDecorated()) {
        block.decorateScope(&sym.addAnonymousScope());
    }
    else {
        SC_ASSERT(block.scope()->kind() != ScopeKind::Anonymous ||
                      currentFunction != nullptr,
                  "If we are analyzing an anonymous scope we must have a "
                  "function pushed, because anonymous scopes "
                  "can only appear in functions.");
    }
    sym.pushScope(block.scope());
    utl::armed_scope_guard popScope = [&] { sym.popScope(); };
    auto statements = block.statements() | ToSmallVector<>;
    for (auto* statement: statements) {
        analyze(*statement);
    }
    popScope.execute();
}

static bool isPoison(ast::Expression* expr) {
    if (!expr || !expr->isDecorated()) {
        return false;
    }
    return isa_or_null<PoisonEntity>(expr->entity());
}

void FuncBodyContext::analyzeImpl(ast::VariableDeclaration& varDecl) {
    SC_ASSERT(currentFunction,
              "We only handle function local variables in this pass.");
    SC_ASSERT(!varDecl.isDecorated(),
              "We should not have handled local variables in prepass.");
    auto* initExpr = analyzeExpr(varDecl.initExpr(), varDecl.dtorStack());
    auto declType = analyzeTypeExpression(varDecl.typeExpr(), ctx);
    auto initType = initExpr ? initExpr->type().get() : nullptr;
    if (initExpr && !initExpr->isValue()) {
        iss.push<BadSymbolReference>(*initExpr, EntityCategory::Value);
        return;
    }
    if (!declType && !initType) {
        sym.declarePoison(std::string(varDecl.name()), EntityCategory::Value);
        if (!isPoison(varDecl.typeExpr()) && !isPoison(initExpr)) {
            iss.push<InvalidDeclaration>(
                &varDecl,
                InvalidDeclaration::Reason::CantInferType,
                sym.currentScope());
        }
        return;
    }
    auto type = declType ? declType : initType;
    if (!isa<ReferenceType>(type) && type->size() == InvalidSize) {
        sym.declarePoison(std::string(varDecl.name()), EntityCategory::Value);
        iss.push<InvalidDeclaration>(&varDecl,
                                     InvalidDeclaration::Reason::InvalidType,
                                     sym.currentScope());
        return;
    }
    if (isa<ReferenceType>(type) && !initExpr) {
        sym.declarePoison(std::string(varDecl.name()), EntityCategory::Value);
        iss.push<InvalidDeclaration>(
            &varDecl,
            InvalidDeclaration::Reason::ExpectedReferenceInitializer,
            sym.currentScope());
        return;
    }
    auto varDeclRes = sym.addVariable(std::string(varDecl.name()),
                                      type,
                                      varDecl.mutability());
    if (!varDeclRes) {
        iss.push(varDeclRes.error()->setStatement(varDecl));
        return;
    }
    auto& variable = *varDeclRes;
    varDecl.decorateVarDecl(&variable);
    if (initExpr) {
        convert(Implicit,
                initExpr,
                variable.getQualType(),
                refToLValue(type),
                varDecl.dtorStack(),
                ctx);
        popTopLevelDtor(initExpr, varDecl.dtorStack());
    }
    else {
        auto call = makeConstructorCall(cast<ObjectType const*>(type),
                                        nullptr,
                                        {},
                                        varDecl.dtorStack(),
                                        ctx,
                                        varDecl.sourceRange());
        if (call) {
            varDecl.setInitExpr(std::move(call));
        }
    }
    if (variable.isConst() && initExpr) {
        variable.setConstantValue(clone(initExpr->constantValue()));
    }
    cast<ast::Statement*>(varDecl.parent())->pushDtor(&variable);
}

void FuncBodyContext::analyzeImpl(ast::ParameterDeclaration& paramDecl) {
    SC_ASSERT(currentFunction != nullptr,
              "We'd better have a function pushed when analyzing function "
              "parameters.");
    SC_ASSERT(!paramDecl.isDecorated(),
              "We should not have handled parameters in prepass.");
    Type const* declaredType =
        currentFunction->function()->argumentType(paramIndex);
    if (declaredType) {
        auto paramRes = sym.addVariable(std::string(paramDecl.name()),
                                        declaredType,
                                        paramDecl.mutability());
        if (!paramRes) {
            iss.push(paramRes.error()->setStatement(paramDecl));
            return;
        }
        paramDecl.decorateVarDecl(&*paramRes);
    }
    else {
        sym.declarePoison(std::string(paramDecl.name()), EntityCategory::Value);
    }
    ++paramIndex;
}

void FuncBodyContext::analyzeImpl(ast::ThisParameter& thisParam) {
    SC_ASSERT(currentFunction != nullptr,
              "We'd better have a function pushed when analyzing function "
              "parameters.");
    SC_ASSERT(!thisParam.isDecorated(),
              "We should not have handled parameters in prepass.");
    auto* function = cast<Function*>(currentFunction->entity());
    auto* parentType = dyncast<ObjectType*>(function->parent());
    if (!parentType) {
        return;
    }
    Type const* type = parentType;
    auto paramRes = [&] {
        if (thisParam.isReference()) {
            type = sym.reference({ parentType, thisParam.mutability() });
            return sym.addVariable("__this", type, Mutability::Const);
        }
        else {
            return sym.addVariable("__this",
                                   parentType,
                                   thisParam.mutability());
        }
    }();
    if (!paramRes) {
        iss.push(paramRes.error()->setStatement(thisParam));
        return;
    }
    function->setIsMember();
    thisParam.decorateVarDecl(&*paramRes);
    ++paramIndex;
}

void FuncBodyContext::analyzeImpl(ast::ExpressionStatement& es) {
    if (sym.currentScope().kind() != ScopeKind::Function) {
        iss.push<InvalidStatement>(
            &es,
            InvalidStatement::Reason::InvalidScopeForStatement,
            sym.currentScope());
        return;
    }
    analyzeExpr(es.expression(), es.dtorStack());
}

void FuncBodyContext::analyzeImpl(ast::ReturnStatement& rs) {
    SC_ASSERT(currentFunction,
              "This should have been set by case FunctionDefinition");
    SC_ASSERT(sym.currentScope().kind() == ScopeKind::Function,
              "Return statements can only occur at function scope. Perhaps "
              "this should be a soft error");
    Type const* returnType = currentFunction->returnType();
    if (!returnType) {
        return;
    }
    if (!rs.expression() && !isa<VoidType>(*returnType)) {
        iss.push<InvalidStatement>(
            &rs,
            InvalidStatement::Reason::NonVoidFunctionMustReturnAValue,
            sym.currentScope());
        return;
    }
    /// We gather parent destructors here because `analyzeExpr()` may add more
    /// constructors and the parent destructors must be lower in the stack
    gatherParentDestructors(rs);
    if (!analyzeExpr(rs.expression(), rs.dtorStack())) {
        return;
    }
    if (isa<VoidType>(*returnType)) {
        iss.push<InvalidStatement>(
            &rs,
            InvalidStatement::Reason::VoidFunctionMustNotReturnAValue,
            sym.currentScope());
        return;
    }
    if (!rs.expression()->isValue()) {
        iss.push<BadSymbolReference>(*rs.expression(), EntityCategory::Value);
        return;
    }
    convert(Implicit,
            rs.expression(),
            getQualType(returnType),
            refToLValue(returnType),
            rs.dtorStack(),
            ctx);
    popTopLevelDtor(rs.expression(), rs.dtorStack());
}

void FuncBodyContext::analyzeImpl(ast::IfStatement& stmt) {
    if (sym.currentScope().kind() != ScopeKind::Function) {
        iss.push<InvalidStatement>(
            &stmt,
            InvalidStatement::Reason::InvalidScopeForStatement,
            sym.currentScope());
        return;
    }
    if (analyzeExpr(stmt.condition(), stmt.dtorStack())) {
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
        iss.push<InvalidStatement>(
            &stmt,
            InvalidStatement::Reason::InvalidScopeForStatement,
            sym.currentScope());
        return;
    }
    stmt.block()->decorateScope(&sym.addAnonymousScope());
    sym.pushScope(stmt.block()->scope());
    if (stmt.varDecl()) {
        analyze(*stmt.varDecl());
    }
    if (analyzeExpr(stmt.condition(), stmt.conditionDtorStack())) {
        convert(Implicit,
                stmt.condition(),
                sym.Bool(),
                RValue,
                stmt.conditionDtorStack(),
                ctx);
    }
    if (stmt.increment()) {
        analyzeExpr(stmt.increment(), stmt.incrementDtorStack());
    }
    sym.popScope(); /// The block will push its scope again.
    analyze(*stmt.block());
}

void FuncBodyContext::analyzeImpl(ast::JumpStatement& stmt) {
    auto* parent = stmt.parent();
    while (true) {
        if (!parent || isa<ast::FunctionDefinition>(parent)) {
            iss.push<InvalidStatement>(&stmt,
                                       InvalidStatement::Reason::InvalidJump,
                                       sym.currentScope());
            return;
        }
        if (isa<ast::LoopStatement>(parent)) {
            break;
        }
        parent = parent->parent();
    }
    gatherParentDestructors(stmt);
}
