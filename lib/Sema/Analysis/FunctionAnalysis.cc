#include "Sema/Analysis/FunctionAnalysis.h"

#include <utl/ranges.hpp>
#include <utl/scope_guard.hpp>

#include "AST/AST.h"
#include "Basic/Basic.h"
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
    void analyze(ast::EmptyStatement&) {}
    void analyze(ast::AbstractSyntaxTree& node) { SC_UNREACHABLE(); }

    ExpressionAnalysisResult dispatchExpression(ast::Expression&);

    void verifyConversion(ast::Expression const& from, TypeID to) const;

    SymbolTable& sym;
    issue::SemaIssueHandler& iss;
    ast::FunctionDefinition* currentFunction = nullptr;
};

} // namespace

void sema::analyzeFunctions(SymbolTable& sym,
                            issue::SemaIssueHandler& iss,
                            std::span<DependencyGraphNode const> functions) {
    Context ctx{ sym, iss };
    for (auto const& node: functions) {
        SC_ASSERT(node.category == SymbolCategory::Function, "We only accept functions here");
        sym.makeScopeCurrent(node.scope);
        ctx.analyze(cast<ast::FunctionDefinition&>(*node.astNode));
        sym.makeScopeCurrent(nullptr);
    }
}

void Context::dispatch(ast::AbstractSyntaxTree& node) {
    visit(node, [this](auto& node) { this->analyze(node); });
}

void Context::analyze(ast::FunctionDefinition& fn) {
    if (auto const sk = sym.currentScope().kind();
        sk != ScopeKind::Global && sk != ScopeKind::Namespace && sk != ScopeKind::Object)
    {
        /// Function defintion is only allowed in the global scope, at namespace
        /// scope and structure scope.
        iss.push(InvalidDeclaration(&fn,
                                    InvalidDeclaration::Reason::InvalidInCurrentScope,
                                    sym.currentScope(),
                                    SymbolCategory::Function));
        return;
    }
    SC_ASSERT(fn.symbolID() != SymbolID::Invalid,
              "Can't analyze the body if wen don't have a symbol to push this functions scope.");
    /// Here the AST node is partially decorated: symbolID() is already set by gatherNames() phase, now we complete the
    /// decoration.
    SymbolID const fnSymID = fn.symbolID();
    auto const& function   = sym.getFunction(fnSymID);
    fn.decorate(fnSymID, function.signature().returnTypeID());
    fn.body->decorate(ScopeKind::Function, function.symbolID());
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
    if (sk != ScopeKind::Global && sk != ScopeKind::Namespace && sk != ScopeKind::Object) {
        /// Function defintion is only allowed in the global scope, at namespace
        /// scope and structure scope.
        iss.push(InvalidDeclaration(&s,
                                    InvalidDeclaration::Reason::InvalidInCurrentScope,
                                    sym.currentScope(),
                                    SymbolCategory::ObjectType));
    }
}

void Context::analyze(ast::CompoundStatement& block) {
    if (!block.isDecorated()) {
        block.decorate(sema::ScopeKind::Anonymous, sym.addAnonymousScope().symbolID());
    }
    else {
        SC_ASSERT(block.scopeKind() != ScopeKind::Anonymous || currentFunction != nullptr,
                  "If we are analyzing an anonymous scope we must have a function pushed, because anonymous scopes "
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
    SC_ASSERT(currentFunction != nullptr, "We only handle function local variables in this pass.");
    SC_ASSERT(!var.isDecorated(), "We should not have handled local variables in prepass.");
    if (!var.typeExpr && !var.initExpression) {
        iss.push(InvalidDeclaration(&var,
                                    InvalidDeclaration::Reason::CantInferType,
                                    sym.currentScope(),
                                    SymbolCategory::Variable));
        return;
    }

    TypeID const declaredTypeID = [&] {
        if (var.typeExpr == nullptr) {
            return TypeID::Invalid;
        }
        auto const varTypeRes = dispatchExpression(*var.typeExpr);
        if (!varTypeRes) {
            return TypeID::Invalid;
        }
        if (varTypeRes.category() != ast::EntityCategory::Type) {
            iss.push(BadSymbolReference(*var.typeExpr, varTypeRes.category(), ast::EntityCategory::Type));
            return TypeID::Invalid;
        }
        auto const& objType = sym.getObjectType(varTypeRes.typeID());
        return objType.symbolID();
    }();
    if (iss.fatal()) {
        return;
    }
    TypeID const deducedTypeID = [&] {
        if (var.initExpression == nullptr) {
            return TypeID::Invalid;
        }
        auto const initExprRes = dispatchExpression(*var.initExpression);
        if (!initExprRes) {
            return TypeID::Invalid;
        }
        if (initExprRes.category() != ast::EntityCategory::Value) {
            iss.push(BadSymbolReference(*var.initExpression, initExprRes.category(), ast::EntityCategory::Value));
            return TypeID::Invalid;
        }
        return var.initExpression->typeID();
    }();
    if (iss.fatal()) {
        return;
    }
    if (!declaredTypeID && !deducedTypeID) {
        return;
    }
    if (declaredTypeID && deducedTypeID) {
        verifyConversion(*var.initExpression, declaredTypeID);
    }
    TypeID const finalTypeID = declaredTypeID ? declaredTypeID : deducedTypeID;
    auto varObj              = sym.addVariable(var, finalTypeID);
    if (!varObj) {
        iss.push(varObj.error());
        return;
    }
    var.decorate(varObj->symbolID(), finalTypeID);
}

void Context::analyze(ast::ParameterDeclaration& paramDecl) {
    SC_ASSERT(currentFunction != nullptr, "We'd better have a function pushed when analyzing function parameters.");
    SC_ASSERT(!paramDecl.isDecorated(), "We should not have handled parameters in prepass.");
    TypeID const declaredTypeID = [&] {
        if (paramDecl.typeExpr == nullptr) {
            return TypeID::Invalid;
        }
        auto const declTypeRes = dispatchExpression(*paramDecl.typeExpr);
        if (!declTypeRes) {
            return TypeID::Invalid;
        }
        if (declTypeRes.category() != ast::EntityCategory::Type) {
            iss.push(BadSymbolReference(*paramDecl.typeExpr, declTypeRes.category(), ast::EntityCategory::Type));
            return TypeID::Invalid;
        }
        auto const& objType = sym.getObjectType(declTypeRes.typeID());
        return objType.symbolID();
    }();
    if (iss.fatal()) {
        return;
    }
    auto paramObj = sym.addVariable(paramDecl.token(), declaredTypeID);
    if (!paramObj) {
        paramObj.error().setStatement(paramDecl);
        iss.push(paramObj.error());
        return;
    }
    paramDecl.decorate(paramObj->symbolID(), declaredTypeID);
}

void Context::analyze(ast::ExpressionStatement& es) {
    if (sym.currentScope().kind() != ScopeKind::Function) {
        iss.push(InvalidStatement(&es, InvalidStatement::Reason::InvalidScopeForStatement, sym.currentScope()));
        return;
    }
    dispatchExpression(*es.expression);
}

void Context::analyze(ast::ReturnStatement& rs) {
    if (sym.currentScope().kind() != ScopeKind::Function) {
        SC_DEBUGFAIL(); // Can this even happen?
        iss.push(InvalidStatement(&rs, InvalidStatement::Reason::InvalidScopeForStatement, sym.currentScope()));
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
        iss.push(BadSymbolReference(*rs.expression, exprRes.category(), ast::EntityCategory::Value));
        return;
    }
    SC_ASSERT(currentFunction != nullptr, "This should have been set by case FunctionDefinition");
    verifyConversion(*rs.expression, currentFunction->returnTypeID());
}

void Context::analyze(ast::IfStatement& is) {
    if (sym.currentScope().kind() != ScopeKind::Function) {
        iss.push(InvalidStatement(&is, InvalidStatement::Reason::InvalidScopeForStatement, sym.currentScope()));
        return;
    }
    if (dispatchExpression(*is.condition)) {
        verifyConversion(*is.condition, sym.Bool());
    }
    if (iss.fatal()) {
        return;
    }
    dispatch(*is.ifBlock);
    if (iss.fatal()) {
        return;
    }

    if (is.elseBlock != nullptr) {
        dispatch(*is.elseBlock);
    }
}

void Context::analyze(ast::WhileStatement& ws) {
    if (sym.currentScope().kind() != ScopeKind::Function) {
        iss.push(InvalidStatement(&ws, InvalidStatement::Reason::InvalidScopeForStatement, sym.currentScope()));
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
    // TODO: This implementation is completely analogous to analyze(WhileStatement&). Try to merge them.
    if (sym.currentScope().kind() != ScopeKind::Function) {
        iss.push(InvalidStatement(&ws, InvalidStatement::Reason::InvalidScopeForStatement, sym.currentScope()));
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
        iss.push(InvalidStatement(&fs, InvalidStatement::Reason::InvalidScopeForStatement, sym.currentScope()));
        return;
    }
    fs.block->decorate(sema::ScopeKind::Anonymous, sym.addAnonymousScope().symbolID());
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

ExpressionAnalysisResult Context::dispatchExpression(ast::Expression& expr) {
    return analyzeExpression(expr, sym, iss);
}

void Context::verifyConversion(ast::Expression const& from, TypeID to) const {
    if (from.typeID() != to) {
        iss.push(BadTypeConversion(from, to));
    }
}
