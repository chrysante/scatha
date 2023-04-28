#include "Sema/Analysis/FunctionAnalysis.h"

#include <utl/scope_guard.hpp>

#include "AST/AST.h"
#include "Common/Base.h"
#include "Sema/Analysis/ExpressionAnalysis.h"
#include "Sema/SemanticIssue.h"

using namespace scatha;
using namespace sema;

namespace {

struct Context {
    void dispatch(ast::AbstractSyntaxTree&);

    void analyze(ast::FunctionDefinition&);
    void analyze(ast::StructDefinition&);
    void analyze(ast::CompoundStatement&);
    void analyze(ast::VariableDeclaration&);
    void analyze(ast::ParameterDeclaration&);
    void analyze(ast::ExpressionStatement&);
    void analyze(ast::ReturnStatement&);
    void analyze(ast::IfStatement&);
    void analyze(ast::WhileStatement&);
    void analyze(ast::DoWhileStatement&);
    void analyze(ast::ForStatement&);
    void analyze(ast::JumpStatement&);
    void analyze(ast::EmptyStatement&) {}
    void analyze(ast::AbstractSyntaxTree& node) { SC_UNREACHABLE(); }

    ExpressionAnalysisResult dispatchExpression(ast::Expression&);

    void verifyConversion(ast::Expression const& from,
                          QualType const* to) const;

    void verifyConversion(ast::Expression const& from, Type const* to) const {
        if (auto* obj = dyncast<ObjectType const*>(to)) {
            verifyConversion(from, sym.qualify(obj));
        }
        else {
            verifyConversion(from, cast<QualType const*>(to));
        }
    }

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
        SC_ASSERT(node.category == SymbolCategory::Function,
                  "We only accept functions here");
        sym.makeScopeCurrent(node.scope);
        ctx.analyze(cast<ast::FunctionDefinition&>(*node.astNode));
        sym.makeScopeCurrent(nullptr);
    }
}

void Context::dispatch(ast::AbstractSyntaxTree& node) {
    visit(node, [this](auto& node) { this->analyze(node); });
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

void Context::analyze(ast::FunctionDefinition& fn) {
    if (auto const sk = sym.currentScope().kind(); sk != ScopeKind::Global &&
                                                   sk != ScopeKind::Namespace &&
                                                   sk != ScopeKind::Object)
    {
        /// Function defintion is only allowed in the global scope, at namespace
        /// scope and structure scope.
        iss.push<InvalidDeclaration>(
            &fn,
            InvalidDeclaration::Reason::InvalidInCurrentScope,
            sym.currentScope(),
            SymbolCategory::Function);
        return;
    }
    SC_ASSERT(fn.symbolID() != SymbolID::Invalid,
              "Can't analyze the body if wen don't have a symbol to push this "
              "functions scope.");
    /// Here the AST node is partially decorated: symbolID() is already set by
    /// gatherNames() phase, now we complete the decoration.
    SymbolID const fnSymID = fn.symbolID();
    auto& function         = sym.get<Function>(fnSymID);
    fn.decorate(fnSymID, function.signature().returnType());
    fn.body->decorate(ScopeKind::Function, function.symbolID());
    function.setAccessSpecifier(translateAccessSpec(fn.accessSpec));
    currentFunction                    = &fn;
    utl::armed_scope_guard popFunction = [&] { currentFunction = nullptr; };
    sym.pushScope(fn.symbolID());
    utl::armed_scope_guard popScope = [&] { sym.popScope(); };
    for (auto& param: fn.parameters) {
        dispatch(*param);
        if (iss.fatal()) {
            return;
        }
    }
    /// The body will push the scope itself again.
    popScope.execute();
    dispatch(*fn.body);
}

void Context::analyze(ast::StructDefinition& s) {
    auto const sk = sym.currentScope().kind();
    if (sk != ScopeKind::Global && sk != ScopeKind::Namespace &&
        sk != ScopeKind::Object)
    {
        /// Function defintion is only allowed in the global scope, at namespace
        /// scope and structure scope.
        iss.push<InvalidDeclaration>(
            &s,
            InvalidDeclaration::Reason::InvalidInCurrentScope,
            sym.currentScope(),
            SymbolCategory::Type);
    }
}

void Context::analyze(ast::CompoundStatement& block) {
    if (!block.isDecorated()) {
        block.decorate(sema::ScopeKind::Anonymous,
                       sym.addAnonymousScope().symbolID());
    }
    else {
        SC_ASSERT(block.scopeKind() != ScopeKind::Anonymous ||
                      currentFunction != nullptr,
                  "If we are analyzing an anonymous scope we must have a "
                  "function pushed, because anonymous scopes "
                  "can only appear in functions.");
    }
    sym.pushScope(block.symbolID());
    utl::armed_scope_guard popScope = [&] { sym.popScope(); };
    for (auto& statement: block.statements) {
        dispatch(*statement);
        if (iss.fatal()) {
            return;
        }
    }
    popScope.execute();
}

void Context::analyze(ast::VariableDeclaration& var) {
    SC_ASSERT(currentFunction != nullptr,
              "We only handle function local variables in this pass.");
    SC_ASSERT(!var.isDecorated(),
              "We should not have handled local variables in prepass.");
    if (!var.typeExpr && !var.initExpression) {
        iss.push<InvalidDeclaration>(&var,
                                     InvalidDeclaration::Reason::CantInferType,
                                     sym.currentScope(),
                                     SymbolCategory::Variable);
        return;
    }

    auto* declaredType = [&]() -> QualType const* {
        if (!var.typeExpr) {
            return nullptr;
        }
        auto const varTypeRes = dispatchExpression(*var.typeExpr);
        if (!varTypeRes) {
            return nullptr;
        }
        if (varTypeRes.category() != ast::EntityCategory::Type) {
            iss.push<BadSymbolReference>(*var.typeExpr,
                                         varTypeRes.category(),
                                         ast::EntityCategory::Type);
            return nullptr;
        }
        return varTypeRes.type();
    }();
    if (iss.fatal()) {
        return;
    }
    auto* deducedType = [&]() -> QualType const* {
        if (!var.initExpression) {
            return nullptr;
        }
        auto const initExprRes = dispatchExpression(*var.initExpression);
        if (!initExprRes) {
            return nullptr;
        }
        if (initExprRes.category() != ast::EntityCategory::Value) {
            iss.push<BadSymbolReference>(*var.initExpression,
                                         initExprRes.category(),
                                         ast::EntityCategory::Value);
            return nullptr;
        }
        return var.initExpression->type();
    }();
    if (iss.fatal()) {
        return;
    }
    if (!declaredType && !deducedType) {
        return;
    }
    if (declaredType && deducedType) {
        verifyConversion(*var.initExpression, declaredType);
    }
    auto* finalType = declaredType ? declaredType : deducedType;
    if (finalType->has(TypeQualifiers::ExplicitReference)) {
        finalType =
            sym.removeQualifiers(finalType, TypeQualifiers::ExplicitReference);
        finalType =
            sym.addQualifiers(finalType, TypeQualifiers::ImplicitReference);
    }
    auto varObj = sym.addVariable(var.nameIdentifier->value(), finalType);
    if (!varObj) {
        iss.push(varObj.error()->setStatement(var));
        return;
    }
    var.decorate(varObj->symbolID(), finalType);
}

void Context::analyze(ast::ParameterDeclaration& paramDecl) {
    SC_ASSERT(currentFunction != nullptr,
              "We'd better have a function pushed when analyzing function "
              "parameters.");
    SC_ASSERT(!paramDecl.isDecorated(),
              "We should not have handled parameters in prepass.");
    auto* declaredType = [&]() -> QualType const* {
        if (!paramDecl.typeExpr) {
            return nullptr;
        }
        auto const declTypeRes = dispatchExpression(*paramDecl.typeExpr);
        if (!declTypeRes) {
            return nullptr;
        }
        if (declTypeRes.category() != ast::EntityCategory::Type) {
            iss.push<BadSymbolReference>(*paramDecl.typeExpr,
                                         declTypeRes.category(),
                                         ast::EntityCategory::Type);
            return nullptr;
        }
        return declTypeRes.type();
    }();
    if (iss.fatal()) {
        return;
    }
    auto paramObj =
        sym.addVariable(paramDecl.nameIdentifier->value(), declaredType);
    if (!paramObj) {
        if (auto* invStatement =
                dynamic_cast<InvalidStatement*>(paramObj.error()))
        {
            invStatement->setStatement(paramDecl);
        }
        iss.push(paramObj.error());
        return;
    }
    paramDecl.decorate(paramObj->symbolID(), declaredType);
}

void Context::analyze(ast::ExpressionStatement& es) {
    if (sym.currentScope().kind() != ScopeKind::Function) {
        iss.push<InvalidStatement>(
            &es,
            InvalidStatement::Reason::InvalidScopeForStatement,
            sym.currentScope());
        return;
    }
    dispatchExpression(*es.expression);
}

void Context::analyze(ast::ReturnStatement& rs) {
    if (sym.currentScope().kind() != ScopeKind::Function) {
        SC_DEBUGFAIL(); // Can this even happen?
        iss.push<InvalidStatement>(
            &rs,
            InvalidStatement::Reason::InvalidScopeForStatement,
            sym.currentScope());
        return;
    }
    if (rs.expression == nullptr) {
        return;
    }
    auto const exprRes = dispatchExpression(*rs.expression);
    if (!exprRes) {
        return;
    }
    if (exprRes.category() != ast::EntityCategory::Value) {
        iss.push<BadSymbolReference>(*rs.expression,
                                     exprRes.category(),
                                     ast::EntityCategory::Value);
        return;
    }
    SC_ASSERT(currentFunction != nullptr,
              "This should have been set by case FunctionDefinition");
    verifyConversion(*rs.expression, currentFunction->returnType());
}

void Context::analyze(ast::IfStatement& is) {
    if (sym.currentScope().kind() != ScopeKind::Function) {
        iss.push<InvalidStatement>(
            &is,
            InvalidStatement::Reason::InvalidScopeForStatement,
            sym.currentScope());
        return;
    }
    if (dispatchExpression(*is.condition)) {
        verifyConversion(*is.condition, sym.Bool());
    }
    if (iss.fatal()) {
        return;
    }
    dispatch(*is.thenBlock);
    if (iss.fatal()) {
        return;
    }
    if (is.elseBlock != nullptr) {
        dispatch(*is.elseBlock);
    }
}

void Context::analyze(ast::WhileStatement& ws) {
    if (sym.currentScope().kind() != ScopeKind::Function) {
        iss.push<InvalidStatement>(
            &ws,
            InvalidStatement::Reason::InvalidScopeForStatement,
            sym.currentScope());
        return;
    }
    if (dispatchExpression(*ws.condition)) {
        verifyConversion(*ws.condition, sym.Bool());
    }
    if (iss.fatal()) {
        return;
    }
    dispatch(*ws.block);
}

void Context::analyze(ast::DoWhileStatement& ws) {
    // TODO: This implementation is completely analogous to
    // analyze(WhileStatement&). Try to merge them.
    if (sym.currentScope().kind() != ScopeKind::Function) {
        iss.push<InvalidStatement>(
            &ws,
            InvalidStatement::Reason::InvalidScopeForStatement,
            sym.currentScope());
        return;
    }
    if (dispatchExpression(*ws.condition)) {
        verifyConversion(*ws.condition, sym.Bool());
    }
    if (iss.fatal()) {
        return;
    }
    dispatch(*ws.block);
}

void Context::analyze(ast::ForStatement& fs) {
    if (sym.currentScope().kind() != ScopeKind::Function) {
        iss.push<InvalidStatement>(
            &fs,
            InvalidStatement::Reason::InvalidScopeForStatement,
            sym.currentScope());
        return;
    }
    fs.block->decorate(sema::ScopeKind::Anonymous,
                       sym.addAnonymousScope().symbolID());
    sym.pushScope(fs.block->symbolID());
    dispatch(*fs.varDecl);
    if (dispatchExpression(*fs.condition)) {
        verifyConversion(*fs.condition, sym.Bool());
    }
    dispatchExpression(*fs.increment);
    if (iss.fatal()) {
        return;
    }
    sym.popScope(); /// The block will push its scope again.
    dispatch(*fs.block);
}

void Context::analyze(ast::JumpStatement& s) {
    /// Need to check if we are in a loop but unfortunately we don't have parent
    /// pointers so it's hard to check.
}

ExpressionAnalysisResult Context::dispatchExpression(ast::Expression& expr) {
    return analyzeExpression(expr, sym, iss);
}

void Context::verifyConversion(ast::Expression const& from,
                               QualType const* to) const {
    if (from.type() != to) {
        iss.push<BadTypeConversion>(from, to);
    }
}
