#include "Sema/Analysis/FunctionAnalysis.h"

#include <utl/scope_guard.hpp>

#include "AST/AST.h"
#include "Common/Base.h"
#include "Sema/Analysis/Conversion.h"
#include "Sema/Analysis/ExpressionAnalysis.h"
#include "Sema/Entity.h"
#include "Sema/SemanticIssue.h"

using namespace scatha;
using namespace sema;

namespace {

struct Context {
    void analyze(ast::AbstractSyntaxTree&);

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
    void analyzeImpl(ast::AbstractSyntaxTree& node) { SC_UNREACHABLE(); }

    bool analyzeExpr(ast::Expression& expr) {
        return sema::analyzeExpression(expr, sym, iss);
    }

    QualType const* getDeclaredType(ast::Expression* expr);

    SymbolTable& sym;
    IssueHandler& iss;
    ast::FunctionDefinition* currentFunction = nullptr;
};

} // namespace

void sema::analyzeFunctions(SymbolTable& sym,
                            IssueHandler& iss,
                            std::span<DependencyGraphNode const> functions) {
    Context ctx{ sym, iss };
    for (auto const& node: functions) {
        SC_ASSERT(isa<Function>(node.entity), "We only accept functions here");
        sym.makeScopeCurrent(node.scope);
        ctx.analyzeImpl(cast<ast::FunctionDefinition&>(*node.astNode));
        sym.makeScopeCurrent(nullptr);
    }
}

void Context::analyze(ast::AbstractSyntaxTree& node) {
    visit(node, [this](auto& node) { this->analyzeImpl(node); });
}

sema::AccessSpecifier translateAccessSpec(ast::AccessSpec spec) {
    switch (spec) {
    case ast::AccessSpec::None:
        return sema::AccessSpecifier::Private;
    case ast::AccessSpec::Public:
        return sema::AccessSpecifier::Public;
    case ast::AccessSpec::Private:
        return sema::AccessSpecifier::Private;
    case ast::AccessSpec::_count:
        SC_UNREACHABLE();
    }
}

void Context::analyzeImpl(ast::FunctionDefinition& fn) {
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
        return;
    }
    SC_ASSERT(fn.function(),
              "Can't analyze the body if wen don't have a symbol to push this "
              "functions scope.");
    /// Here the AST node is partially decorated: `symbolID()` is already set by
    /// `gatherNames()` phase, now we complete the decoration.
    auto* function = fn.function();
    fn.decorate(function, function->returnType());
    fn.body()->decorate(function);
    function->setAccessSpecifier(translateAccessSpec(fn.accessSpec()));
    currentFunction                    = &fn;
    utl::armed_scope_guard popFunction = [&] { currentFunction = nullptr; };
    sym.pushScope(function);
    utl::armed_scope_guard popScope = [&] { sym.popScope(); };
    for (auto* param: fn.parameters()) {
        analyze(*param);
    }
    /// The body will push the scope itself again.
    popScope.execute();
    analyze(*fn.body());
}

void Context::analyzeImpl(ast::StructDefinition& s) {
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
    }
}

void Context::analyzeImpl(ast::CompoundStatement& block) {
    if (!block.isDecorated()) {
        block.decorate(&sym.addAnonymousScope());
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
    for (auto* statement: block.statements()) {
        analyze(*statement);
    }
    popScope.execute();
}

void Context::analyzeImpl(ast::VariableDeclaration& var) {
    SC_ASSERT(currentFunction,
              "We only handle function local variables in this pass.");
    SC_ASSERT(!var.isDecorated(),
              "We should not have handled local variables in prepass.");
    if (!var.typeExpr() && !var.initExpression()) {
        iss.push<InvalidDeclaration>(&var,
                                     InvalidDeclaration::Reason::CantInferType,
                                     sym.currentScope());
        return;
    }
    auto* declaredType = getDeclaredType(var.typeExpr());
    auto* deducedType  = [&]() -> QualType const* {
        if (!var.initExpression() || !analyzeExpr(*var.initExpression())) {
            return nullptr;
        }
        if (!var.initExpression()->isValue()) {
            iss.push<BadSymbolReference>(*var.initExpression(),
                                         EntityCategory::Value);
            return nullptr;
        }
        return var.initExpression()->type();
    }();
    if (!declaredType && !deducedType) {
        /// FIXME: Push error here
        /// Or instead declare a poison entity to prevent errors on references
        /// to this variable
        return;
    }
    if (declaredType && deducedType && declaredType->isReference() &&
        !deducedType->isExplicitRef())
    {
        iss.push<InvalidDeclaration>(
            &var,
            InvalidDeclaration::Reason::ExpectedReferenceInitializer,
            sym.currentScope());
        return;
    }
    auto* finalType = declaredType ? declaredType :
                      deducedType->isExplicitRef() ?
                                     deducedType :
                                     sym.qualify(deducedType->base());

    if (var.initExpression() && var.initExpression()->isDecorated()) {
        convertImplicitly(var.initExpression(), finalType, iss);
    }
    auto varRes = sym.addVariable(std::string(var.name()), finalType);
    if (!varRes) {
        iss.push(varRes.error()->setStatement(var));
        return;
    }
    auto& varObj = *varRes;
    var.decorate(&varObj, finalType);
}

void Context::analyzeImpl(ast::ParameterDeclaration& paramDecl) {
    SC_ASSERT(currentFunction != nullptr,
              "We'd better have a function pushed when analyzing function "
              "parameters.");
    SC_ASSERT(!paramDecl.isDecorated(),
              "We should not have handled parameters in prepass.");
    auto* declaredType = getDeclaredType(paramDecl.typeExpr());
    auto paramRes =
        sym.addVariable(std::string(paramDecl.name()), declaredType);
    if (!paramRes) {
        iss.push(paramRes.error()->setStatement(paramDecl));
        return;
    }
    auto& param = *paramRes;
    paramDecl.decorate(&param, declaredType);
}

void Context::analyzeImpl(ast::ThisParameter& thisParam) {
    SC_ASSERT(currentFunction != nullptr,
              "We'd better have a function pushed when analyzing function "
              "parameters.");
    SC_ASSERT(!thisParam.isDecorated(),
              "We should not have handled parameters in prepass.");
    auto* function   = cast<Function*>(currentFunction->entity());
    auto* parentType = dyncast<ObjectType*>(function->parent());
    if (!parentType) {
        return;
    }
    auto* qualType = sym.qualify(parentType, thisParam.reference());
    auto paramRes  = sym.addVariable("__this", qualType);
    if (!paramRes) {
        iss.push(paramRes.error()->setStatement(thisParam));
        return;
    }
    function->setIsMember();
    auto& param = *paramRes;
    thisParam.decorate(&param, qualType);
}

void Context::analyzeImpl(ast::ExpressionStatement& es) {
    if (sym.currentScope().kind() != ScopeKind::Function) {
        iss.push<InvalidStatement>(
            &es,
            InvalidStatement::Reason::InvalidScopeForStatement,
            sym.currentScope());
        return;
    }
    analyzeExpr(*es.expression());
}

void Context::analyzeImpl(ast::ReturnStatement& rs) {
    SC_ASSERT(currentFunction,
              "This should have been set by case FunctionDefinition");
    if (sym.currentScope().kind() != ScopeKind::Function) {
        SC_DEBUGFAIL(); // Can this even happen?
        iss.push<InvalidStatement>(
            &rs,
            InvalidStatement::Reason::InvalidScopeForStatement,
            sym.currentScope());
        return;
    }
    auto* returnType = currentFunction->returnType();
    if (!returnType) {
        return;
    }
    if (!rs.expression() && returnType->base() != sym.Void()) {
        iss.push<InvalidStatement>(
            &rs,
            InvalidStatement::Reason::NonVoidFunctionMustReturnAValue,
            sym.currentScope());
        return;
    }
    if (rs.expression() && returnType->base() == sym.Void()) {
        iss.push<InvalidStatement>(
            &rs,
            InvalidStatement::Reason::VoidFunctionMustNotReturnAValue,
            sym.currentScope());
        return;
    }
    if (!rs.expression() || !analyzeExpr(*rs.expression())) {
        return;
    }
    if (!rs.expression()->isValue()) {
        iss.push<BadSymbolReference>(*rs.expression(), EntityCategory::Value);
        return;
    }
    /// This is specified to returns.
    /// If we return a reference we don't want to _assign through_ it but we
    /// want to return an explicit reference
    if (returnType->isReference() && !rs.expression()->type()->isExplicitRef())
    {
        iss.push<BadExpression>(*rs.expression(), IssueSeverity::Error);
        return;
    }
    convertImplicitly(rs.expression(), returnType, iss);
}

void Context::analyzeImpl(ast::IfStatement& stmt) {
    if (sym.currentScope().kind() != ScopeKind::Function) {
        iss.push<InvalidStatement>(
            &stmt,
            InvalidStatement::Reason::InvalidScopeForStatement,
            sym.currentScope());
        return;
    }
    if (analyzeExpr(*stmt.condition())) {
        convertImplicitly(stmt.condition(), sym.qBool(), iss);
    }
    analyze(*stmt.thenBlock());
    if (stmt.elseBlock()) {
        analyze(*stmt.elseBlock());
    }
}

void Context::analyzeImpl(ast::LoopStatement& stmt) {
    if (sym.currentScope().kind() != ScopeKind::Function) {
        iss.push<InvalidStatement>(
            &stmt,
            InvalidStatement::Reason::InvalidScopeForStatement,
            sym.currentScope());
        return;
    }
    stmt.block()->decorate(&sym.addAnonymousScope());
    sym.pushScope(stmt.block()->scope());
    if (stmt.varDecl()) {
        analyze(*stmt.varDecl());
    }
    if (analyzeExpr(*stmt.condition())) {
        convertImplicitly(stmt.condition(), sym.qBool(), iss);
    }
    if (stmt.increment()) {
        analyzeExpr(*stmt.increment());
    }
    sym.popScope(); /// The block will push its scope again.
    analyze(*stmt.block());
}

void Context::analyzeImpl(ast::JumpStatement& s) {
    /// Need to check if we are in a loop but unfortunately we don't have parent
    /// pointers so it's hard to check.
}

QualType const* Context::getDeclaredType(ast::Expression* expr) {
    if (!expr || !analyzeExpr(*expr)) {
        return nullptr;
    }
    if (!expr->isType()) {
        iss.push<BadSymbolReference>(*expr, EntityCategory::Type);
        return nullptr;
    }
    return cast<QualType*>(expr->entity());
}
