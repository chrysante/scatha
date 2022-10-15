#define UTL_DEFER_MACROS

#include "Sema/Analyze.h"

#include <sstream>
#include <utl/scope_guard.hpp>
#include <utl/strcat.hpp>

#include "AST/AST.h"
#include "AST/Visit.h"
#include "Basic/Basic.h"
#include "Sema/ExpressionAnalysis.h"
#include "Sema/Prepass.h"
#include "Sema/SemanticIssue.h"

namespace scatha::sema {

using namespace ast;

namespace {
struct Context {
    void dispatch(AbstractSyntaxTree&);

    void analyze(TranslationUnit& tu);
    void analyze(Block&);
    void analyze(FunctionDefinition&);
    void analyze(StructDefinition&);
    void analyze(VariableDeclaration&);
    void analyze(ExpressionStatement&);
    void analyze(ReturnStatement&);
    void analyze(IfStatement&);
    void analyze(WhileStatement&);
    void analyze(AbstractSyntaxTree&) { SC_DEBUGFAIL(); }

    ExpressionAnalysisResult dispatchExpression(Expression&);

    void verifyConversion(Expression const& from, TypeID to) const;

    SymbolTable& sym;
    issue::IssueHandler& iss;
    ast::FunctionDefinition* currentFunction = nullptr;
};
} // namespace

SymbolTable analyze(AbstractSyntaxTree* root, issue::IssueHandler& iss) {
    SymbolTable sym = prepass(*root, iss);
    Context ctx{ sym, iss };
    ctx.dispatch(*root);
    return sym;
}

void Context::dispatch(AbstractSyntaxTree& node) {
    ast::visit(node, [this](auto& node) { this->analyze(node); });
}

void Context::analyze(TranslationUnit& tu) {
    for (auto& decl : tu.declarations) {
        dispatch(*decl);
        if (iss.fatal()) {
            return;
        }
    }
}

void Context::analyze(Block& block) {
    if (block.scopeKind == ScopeKind::Anonymous) {
        if (currentFunction == nullptr) {
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

void Context::analyze(FunctionDefinition& fn) {
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
    if (fn.symbolID == SymbolID::Invalid) {
        /// Can't analyze the body if wen don't have a symbol to push this
        /// functions scope.
        return;
    }
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

void Context::analyze(StructDefinition& s) {
    if (auto const sk = sym.currentScope().kind();
        sk != ScopeKind::Global && sk != ScopeKind::Namespace && sk != ScopeKind::Object) {
        iss.push(InvalidDeclaration(&s,
                                    InvalidDeclaration::Reason::InvalidInCurrentScope,
                                    sym.currentScope(),
                                    SymbolCategory::ObjectType));
        return;
    }
    dispatch(*s.body);
}

void Context::analyze(VariableDeclaration& var) {
    if (var.symbolID) {
        /// We already handled this variable in prepass.
        var.typeID = sym.getVariable(var.symbolID).typeID();
        return;
    }
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
        if (varTypeRes.category() != EntityCategory::Type) {
            iss.push(BadSymbolReference(*var.typeExpr, varTypeRes.category(), EntityCategory::Type));
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
    auto varObj = sym.addVariable(var.token(), var.typeID, var.offset);
    if (!varObj) {
        varObj.error().setStatement(var);
        iss.push(varObj.error());
        return;
    }
    var.symbolID = varObj->symbolID();
}

void Context::analyze(ExpressionStatement& es) {
    if (sym.currentScope().kind() != ScopeKind::Function) {
        iss.push(InvalidStatement(&es, InvalidStatement::Reason::InvalidScopeForStatement, sym.currentScope()));
        return;
    }
    dispatchExpression(*es.expression);
}

void Context::analyze(ReturnStatement& rs) {
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

void Context::analyze(IfStatement& is) {
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

void Context::analyze(WhileStatement& ws) {
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

ExpressionAnalysisResult Context::dispatchExpression(Expression& expr) {
    return analyzeExpression(expr, sym, &iss);
}

void Context::verifyConversion(Expression const& from, TypeID to) const {
    if (from.typeID != to) {
        iss.push(BadTypeConversion(from, to));
    }
}

} // namespace scatha::sema
