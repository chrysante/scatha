#include "Sema/Analysis/GatherNames.h"

#include <utl/vector.hpp>

#include <utl/scope_guard.hpp>

#include "AST/AST.h"
#include "AST/Visit.h"
#include "Sema/ExpressionAnalysis.h"
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
    size_t gather(ast::AbstractSyntaxTree&) { SC_UNREACHABLE(); }

    SymbolTable& sym;
    issue::IssueHandler& iss;
    utl::vector<DependencyGraphNode>& dependencyGraph;
};

} // namespace

utl::vector<DependencyGraphNode> scatha::sema::gatherNames(SymbolTable& sym,
                                                           ast::AbstractSyntaxTree& root,
                                                           issue::IssueHandler& iss)
{
    utl::vector<DependencyGraphNode> dependencyGraph;
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
    Expected const overloadSet = sym.declareFunction(fn.token());
    if (!overloadSet.hasValue()) {
        auto error = overloadSet.error();
        error.setStatement(fn);
        iss.push(error);
        return invalidIndex;
    }
    fn.overloadSetID = overloadSet->symbolID();
    /// Now add this function definition to the dependency graph
    size_t const index = dependencyGraph.size();
    dependencyGraph.push_back({
        .symbolID = SymbolID::Invalid,
        .category = SymbolCategory::Function,
        .astNode = &fn,
        .scope = &sym.currentScope()
    });
    return index;
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
    /// After we declared this type we gather all its members
    utl::small_vector<u16> dependencies;
    sym.pushScope(objType->symbolID());
    utl::armed_scope_guard popScope = [&] { sym.popScope(); };
    for (auto& statement: s.body->statements) {
        size_t const index = dispatch(*statement);
        dependencies.push_back(utl::narrow_cast<u16>(index));
    }
    popScope.execute();
    /// Now add this struct definition to the dependency graph
    size_t const index = dependencyGraph.size();
    dependencyGraph.push_back({
        .symbolID = objType->symbolID(),
        .category = SymbolCategory::ObjectType,
        .astNode = &s,
        .scope = &sym.currentScope(),
        .dependencies = dependencies
    });
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
    size_t const index = dependencyGraph.size();
    dependencyGraph.push_back({
        .symbolID = var.symbolID(),
        .category = SymbolCategory::Variable,
        .astNode = &varDecl,
        .scope = &sym.currentScope()
    });
    return index;
}
