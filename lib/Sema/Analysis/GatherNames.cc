#include "Sema/Analysis/GatherNames.h"

#include <utl/vector.hpp>

#include <utl/scope_guard.hpp>

#include "AST/AST.h"
#include "Sema/Analysis/ExpressionAnalysis.h"
#include "Sema/SemaIssue2.h"

using namespace scatha;
using namespace scatha::sema;

namespace {

/// Gathers all declarations and declares them in the symbol table. Also
/// analyzes the dependencies of structs because they are trivial.
struct Context {
    /// Dispatches to the appropriate one of the `gather()` overloads below
    /// based on the runtime type of \p node
    size_t dispatch(ast::AbstractSyntaxTree& node);

    size_t gather(ast::TranslationUnit&);
    size_t gather(ast::FunctionDefinition&);
    size_t gather(ast::StructDefinition&);
    size_t gather(ast::VariableDeclaration&);
    size_t gather(ast::Statement&);
    size_t gather(ast::AbstractSyntaxTree&) { SC_UNREACHABLE(); }

    SymbolTable& sym;
    IssueHandler& iss;
    DependencyGraph& dependencyGraph;
};

} // namespace

DependencyGraph scatha::sema::gatherNames(SymbolTable& sym,
                                          ast::AbstractSyntaxTree& root,
                                          IssueHandler& iss) {
    DependencyGraph dependencyGraph;
    Context ctx{ sym, iss, dependencyGraph };
    ctx.dispatch(root);
    return dependencyGraph;
}

size_t Context::dispatch(ast::AbstractSyntaxTree& node) {
    return visit(node, [this](auto& node) { return this->gather(node); });
}

size_t Context::gather(ast::TranslationUnit& tu) {
    for (auto& decl: tu.declarations) {
        dispatch(*decl);
    }
    return invalidIndex;
}

size_t Context::gather(ast::FunctionDefinition& funcDef) {
    if (auto const scopeKind = sym.currentScope().kind();
        scopeKind != ScopeKind::Global && scopeKind != ScopeKind::Namespace &&
        scopeKind != ScopeKind::Object)
    {
        /// Function defintion is only allowed in the global scope, at namespace
        /// scope and structure scope
        iss.push<InvalidDeclaration>(
            &funcDef,
            InvalidDeclaration::Reason::InvalidInCurrentScope,
            sym.currentScope(),
            SymbolCategory::Function);
        return static_cast<size_t>(-1);
    }
    Expected const declResult = sym.declareFunction(funcDef);
    if (!declResult.hasValue()) {
        iss.push(declResult.error());
        return invalidIndex;
    }
    auto& funcObj = *declResult;
    funcDef.Declaration::decorate(funcObj.symbolID());
    funcDef.body->decorate(ScopeKind::Function, funcObj.symbolID());
    /// Now add this function definition to the dependency graph
    return dependencyGraph.add({ .symbolID = funcObj.symbolID(),
                                 .category = SymbolCategory::Function,
                                 .astNode  = &funcDef,
                                 .scope    = &sym.currentScope() });
}

size_t Context::gather(ast::StructDefinition& s) {
    if (auto const sk = sym.currentScope().kind(); sk != ScopeKind::Global &&
                                                   sk != ScopeKind::Namespace &&
                                                   sk != ScopeKind::Object)
    {
        /// Struct defintion is only allowed in the global scope, at namespace
        /// scope and structure scope
        iss.push<InvalidDeclaration>(
            &s,
            InvalidDeclaration::Reason::InvalidInCurrentScope,
            sym.currentScope(),
            SymbolCategory::ObjectType);
        return invalidIndex;
    }
    Expected const declResult = sym.declareObjectType(s);
    if (!declResult) {
        iss.push(declResult.error());
        return invalidIndex;
    }
    auto& objType = *declResult;
    s.decorate(objType.symbolID());
    s.body->decorate(ScopeKind::Object, objType.symbolID());
    SC_ASSERT(s.symbolID() != SymbolID::Invalid, "");
    size_t const index =
        dependencyGraph.add({ .symbolID = objType.symbolID(),
                              .category = SymbolCategory::ObjectType,
                              .astNode  = &s,
                              .scope    = &sym.currentScope() });
    /// After we declared this type we gather all its members
    sym.pushScope(objType.symbolID());
    utl::armed_scope_guard popScope = [&] { sym.popScope(); };
    for (auto& statement: s.body->statements) {
        size_t const dependency = dispatch(*statement);
        if (dependency != invalidIndex) {
            dependencyGraph[index].dependencies.push_back(
                utl::narrow_cast<u16>(dependency));
        }
    }
    popScope.execute();
    /// Now add this struct definition to the dependency graph
    return index;
}

size_t Context::gather(ast::VariableDeclaration& varDecl) {
    SC_ASSERT(
        sym.currentScope().kind() == ScopeKind::Object,
        "We only want to prepass struct definitions. What are we doing here?");
    SC_ASSERT(varDecl.typeExpr,
              "In structs variables need explicit type "
              "specifiers. Make this a program issue.");
    auto declResult = sym.declareVariable(varDecl);
    if (!declResult) {
        iss.push(declResult.error());
        return invalidIndex;
    }
    auto const& var = *declResult;
    return dependencyGraph.add({ .symbolID = var.symbolID(),
                                 .category = SymbolCategory::Variable,
                                 .astNode  = &varDecl,
                                 .scope    = &sym.currentScope() });
}

size_t Context::gather(ast::Statement& statement) {
    using enum InvalidStatement::Reason;
    iss.push<InvalidStatement>(&statement,
                               InvalidScopeForStatement,
                               sym.currentScope());
    return invalidIndex;
}
