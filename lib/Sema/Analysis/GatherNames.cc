#include "Sema/Analysis/GatherNames.h"

#include <utl/vector.hpp>

#include <utl/scope_guard.hpp>

#include "AST/AST.h"
#include "AST/Visit.h"
#include "Sema/Analysis/ExpressionAnalysis.h"
#include "Sema/SemanticIssue.h"

using namespace scatha;
using namespace scatha::sema;

namespace {

/// Gathers all declarations and declares them in the symbol table. Also analyzes the dependencies of structs because they are trivial.
struct Context {
    /// Dispatches to the appropriate one of the \p gather() overloads below based on the runtime type of \p node
    size_t dispatch(ast::AbstractSyntaxTree& node);

    size_t gather(ast::TranslationUnit&);
    size_t gather(ast::FunctionDefinition&);
    size_t gather(ast::StructDefinition&);
    size_t gather(ast::VariableDeclaration&);
    size_t gather(ast::Statement&);
    size_t gather(ast::AbstractSyntaxTree&) { SC_UNREACHABLE(); }

    SymbolTable& sym;
    issue::IssueHandler& iss;
    DependencyGraph& dependencyGraph;
};

} // namespace

DependencyGraph scatha::sema::gatherNames(SymbolTable& sym,
                                          ast::AbstractSyntaxTree& root,
                                          issue::IssueHandler& iss)
{
    DependencyGraph dependencyGraph;
    Context ctx{ sym, iss, dependencyGraph };
    ctx.dispatch(root);
    return dependencyGraph;
}

size_t Context::dispatch(ast::AbstractSyntaxTree& node) {
    return visit(node, [this](auto& node) { return this->gather(node); });
}

size_t Context::gather(ast::TranslationUnit& tu) {
    for (auto& decl : tu.declarations) {
        dispatch(*decl);
    }
    return invalidIndex;
}

size_t Context::gather(ast::FunctionDefinition& fn) {
    if (auto const sk = sym.currentScope().kind();
        sk != ScopeKind::Global && sk != ScopeKind::Namespace && sk != ScopeKind::Object) {
        /// Function defintion is only allowed in the global scope, at namespace
        /// scope and structure scope
        iss.push(InvalidDeclaration(&fn,
                                    InvalidDeclaration::Reason::InvalidInCurrentScope,
                                    sym.currentScope(),
                                    SymbolCategory::Function));
        return (size_t)-1;
    }
    Expected const func = sym.declareFunction(fn.token());
    if (!func.hasValue()) {
        auto error = func.error();
        error.setStatement(fn);
        iss.push(error);
        return invalidIndex;
    }
    fn.symbolID = func->symbolID();
    fn.body->scopeKind = ScopeKind::Function;
    /// Now add this function definition to the dependency graph
    return dependencyGraph.add({
        .symbolID = func->symbolID(),
        .category = SymbolCategory::Function,
        .astNode = &fn,
        .scope = &sym.currentScope()
    });
}

size_t Context::gather(ast::StructDefinition& s) {
    if (auto const sk = sym.currentScope().kind();
        sk != ScopeKind::Global && sk != ScopeKind::Namespace && sk != ScopeKind::Object) {
        /// Struct defintion is only allowed in the global scope, at namespace
        /// scope and structure scope
        iss.push(InvalidDeclaration(&s,
                                    InvalidDeclaration::Reason::InvalidInCurrentScope,
                                    sym.currentScope(),
                                    SymbolCategory::ObjectType));
        return invalidIndex;
    }
    Expected const objType = sym.declareObjectType(s.token());
    if (!objType) {
        auto error = objType.error();
        error.setStatement(s);
        iss.push(error);
        return invalidIndex;
    }
    s.symbolID = objType->symbolID();
    s.body->scopeKind = ScopeKind::Object;
    SC_ASSERT(s.symbolID != SymbolID::Invalid, "");
    size_t const index = dependencyGraph.add({
        .symbolID = objType->symbolID(),
        .category = SymbolCategory::ObjectType,
        .astNode = &s,
        .scope = &sym.currentScope()
    });
    /// After we declared this type we gather all its members
    sym.pushScope(objType->symbolID());
    utl::armed_scope_guard popScope = [&] { sym.popScope(); };
    for (auto& statement: s.body->statements) {
        size_t const dependency = dispatch(*statement);
        if (dependency != invalidIndex) {
            dependencyGraph[index].dependencies.push_back(utl::narrow_cast<u16>(dependency));            
        }
    }
    popScope.execute();
    /// Now add this struct definition to the dependency graph
    return index;
}

size_t Context::gather(ast::VariableDeclaration& varDecl) {
    SC_ASSERT(sym.currentScope().kind() == ScopeKind::Object,
              "We only want to prepass struct definitions. What are we doing here?");
    SC_ASSERT(varDecl.typeExpr,
              "In structs variables need explicit type "
              "specifiers. Make this a program issue.");
    auto declResult = sym.declareVariable(varDecl.token());
    if (!declResult) {
        auto error = declResult.error();
        error.setStatement(varDecl);
        iss.push(error);
        return invalidIndex;
    }
    auto const& var = *declResult;
    return dependencyGraph.add({
        .symbolID = var.symbolID(),
        .category = SymbolCategory::Variable,
        .astNode = &varDecl,
        .scope = &sym.currentScope()
    });
}

size_t Context::gather(ast::Statement& statement) {
    using enum InvalidStatement::Reason;
    iss.push(SemanticIssue{
        InvalidStatement(&statement, InvalidScopeForStatement, sym.currentScope())
    });
    return invalidIndex;
}
