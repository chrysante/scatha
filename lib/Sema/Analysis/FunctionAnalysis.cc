#include "Sema/Analysis/FunctionAnalysis.h"

#include <utl/scope_guard.hpp>
#include <utl/ranges.hpp>

#include "AST/AST.h"
#include "AST/Visit.h"
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
    void analyze(ast::Block&);
    void analyze(ast::VariableDeclaration&);
    void analyze(ast::ExpressionStatement&);
    void analyze(ast::ReturnStatement&);
    void analyze(ast::IfStatement&);
    void analyze(ast::WhileStatement&);
    void analyze(ast::AbstractSyntaxTree&) { SC_UNREACHABLE(); }

    ExpressionAnalysisResult dispatchExpression(ast::Expression&);
    
    void verifyConversion(ast::Expression const& from, TypeID to) const;

    SymbolTable& sym;
    issue::IssueHandler& iss;
    ast::FunctionDefinition* currentFunction = nullptr;
};

} // namespace

void sema::analyzeFunctions(SymbolTable& sym,
                            issue::IssueHandler& iss,
                            std::span<DependencyGraphNode const> functions)
{
    Context ctx{ sym, iss };
    for (auto const& node: functions) {
        SC_ASSERT(node.category == SymbolCategory::Function, "We only accept functions here");
        sym.makeScopeCurrent(node.scope);
        ctx.analyze(utl::down_cast<ast::FunctionDefinition&>(*node.astNode));
        sym.makeScopeCurrent(nullptr);
    }
}

void Context::dispatch(ast::AbstractSyntaxTree& node) {
    ast::visit(node, [this](auto& node) { this->analyze(node); });
}

void Context::analyze(ast::FunctionDefinition& fn) {
    if (auto const sk = sym.currentScope().kind();
        sk != ScopeKind::Global && sk != ScopeKind::Namespace && sk != ScopeKind::Object) {
        /// Function defintion is only allowed in the global scope, at namespace
        /// scope and structure scope.
        iss.push(InvalidDeclaration(&fn,
                                    InvalidDeclaration::Reason::InvalidInCurrentScope,
                                    sym.currentScope(),
                                    SymbolCategory::Function));
        return;
    }
    SC_ASSERT(fn.symbolID != SymbolID::Invalid,
              "Can't analyze the body if wen don't have a symbol to push this functions scope.");
    auto const& function = sym.getFunction(fn.symbolID);
    fn.returnTypeID = function.signature().returnTypeID();
    fn.body->scopeSymbolID = function.symbolID();
    currentFunction                    = &fn;
    utl::armed_scope_guard popFunction = [&] { currentFunction = nullptr; };
    sym.pushScope(fn.symbolID);
    utl::armed_scope_guard popScope = [&] { sym.popScope(); };
    for (auto& param : fn.parameters) {
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
    if (sk != ScopeKind::Global &&
        sk != ScopeKind::Namespace &&
        sk != ScopeKind::Object)
    {
        /// Function defintion is only allowed in the global scope, at namespace
        /// scope and structure scope.
        iss.push(InvalidDeclaration(&s,
                                    InvalidDeclaration::Reason::InvalidInCurrentScope,
                                    sym.currentScope(),
                                    SymbolCategory::ObjectType));
    }
}

void Context::analyze(ast::Block& block) {
    if (block.scopeKind == ScopeKind::Anonymous) {
        if (currentFunction == nullptr) {
            SC_DEBUGFAIL(); /// Can this case still happen when the current design?
            /// Anonymous blocks may only appear at function scope.
            iss.push(InvalidStatement(&block, InvalidStatement::Reason::InvalidScopeForStatement, sym.currentScope()));
            return;
        }
        block.scopeSymbolID = sym.addAnonymousScope().symbolID();
    }
    sym.pushScope(block.scopeSymbolID);
    utl::armed_scope_guard popScope = [&] { sym.popScope(); };
    for (auto& statement : block.statements) {
        dispatch(*statement);
        if (iss.fatal()) {
            return;
        }
    }
    popScope.execute();
}

void Context::analyze(ast::VariableDeclaration& var) {
    SC_ASSERT(!var.symbolID, "We should not handle local variables in prepass.");
    if (!var.typeExpr && !var.initExpression) {
        iss.push(InvalidDeclaration(&var,
                                    InvalidDeclaration::Reason::CantInferType,
                                    sym.currentScope(),
                                    SymbolCategory::Variable));
        return;
    }
    if (var.typeExpr) {
        auto const varTypeRes = dispatchExpression(*var.typeExpr);
        if (iss.fatal()) {
            return;
        }
        if (!varTypeRes) {
            goto expression;
        }
        if (varTypeRes.category() != ast::EntityCategory::Type) {
            iss.push(BadSymbolReference(*var.typeExpr, varTypeRes.category(), ast::EntityCategory::Type));
            goto expression;
        }
        auto const& objType = sym.getObjectType(varTypeRes.typeID());
        var.typeID          = objType.symbolID();
    }
expression:
    if (var.initExpression) {
        auto const initExprRes = dispatchExpression(*var.initExpression);
        if (!initExprRes) {
            goto declaration;
        }
        if (initExprRes.category() != ast::EntityCategory::Value) {
            iss.push(BadSymbolReference(*var.initExpression, initExprRes.category(), ast::EntityCategory::Value));
            goto declaration;
        }
        if (!var.typeID) {
            /// Deduce variable type from init expression.
            var.typeID = var.initExpression->typeID;
            goto declaration;
        }
        /// We already have the type from the explicit type specifier.
        verifyConversion(*var.initExpression, var.typeID);
    }
declaration:
    auto varObj = sym.addVariable(var, var.typeID, var.offset);
    if (!varObj) {
        iss.push(varObj.error());
        return;
    }
    var.symbolID = varObj->symbolID();
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
        iss.push(InvalidStatement(&rs, InvalidStatement::Reason::InvalidScopeForStatement, sym.currentScope()));
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
    verifyConversion(*rs.expression, currentFunction->returnTypeID);
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

ExpressionAnalysisResult Context::dispatchExpression(ast::Expression& expr) {
    return analyzeExpression(expr, sym, iss);
}

void Context::verifyConversion(ast::Expression const& from, TypeID to) const {
    if (from.typeID != to) {
        iss.push(BadTypeConversion(from, to));
    }
}
