#include "Sema/Analysis/GatherNames.h"

#include <utl/vector.hpp>

#include <utl/scope_guard.hpp>

#include "AST/AST.h"
#include "Sema/Analysis/ExpressionAnalysis.h"
#include "Sema/Entity.h"
#include "Sema/SemanticIssue.h"

using namespace scatha;
using namespace scatha::sema;

static constexpr size_t InvalidIndex = ~size_t{};

namespace {

/// Gathers all declarations and declares them in the symbol table. Also
/// analyzes the dependencies of structs because they are trivial.
struct Context {
    /// Dispatches to the appropriate one of the `gather()` overloads below
    /// based on the runtime type of \p node
    size_t dispatch(ast::ASTNode& node);

    size_t gather(ast::TranslationUnit&);
    size_t gather(ast::FunctionDefinition&);
    size_t gather(ast::StructDefinition&);
    size_t gather(ast::VariableDeclaration&);
    size_t gather(ast::Statement&);
    size_t gather(ast::ASTNode&) { SC_UNREACHABLE(); }

    SymbolTable& sym;
    IssueHandler& iss;
    StructDependencyGraph& dependencyGraph;
    std::vector<ast::FunctionDefinition*>& functions;
};

} // namespace

GatherNamesResult scatha::sema::gatherNames(SymbolTable& sym,
                                            ast::ASTNode& root,
                                            IssueHandler& iss) {
    GatherNamesResult result;
    Context ctx{ sym, iss, result.structs, result.functions };
    ctx.dispatch(root);
    return result;
}

size_t Context::dispatch(ast::ASTNode& node) {
    return visit(node, [this](auto& node) { return this->gather(node); });
}

size_t Context::gather(ast::TranslationUnit& tu) {
    for (auto* decl: tu.declarations()) {
        dispatch(*decl);
    }
    return InvalidIndex;
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
            sym.currentScope());
        return static_cast<size_t>(-1);
    }
    Expected const declResult =
        sym.declareFunction(std::string(funcDef.name()));
    if (!declResult.hasValue()) {
        iss.push(declResult.error()->setStatement(funcDef));
        return InvalidIndex;
    }
    auto& func = *declResult;
    funcDef.Declaration::decorate(&func);
    funcDef.body()->decorate(&func);
    /// Now add this function definition to the dependency graph
    functions.push_back(&funcDef);
    return ~size_t{};
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
            sym.currentScope());
        return InvalidIndex;
    }
    Expected const declResult = sym.declareStructureType(std::string(s.name()));
    if (!declResult) {
        iss.push(declResult.error()->setStatement(s));
        return InvalidIndex;
    }
    auto& objType = *declResult;
    s.decorate(&objType);
    s.body()->decorate(&objType);
    size_t const index =
        dependencyGraph.add({ .entity = &objType, .astNode = &s });
    /// After we declared this type we gather all its members
    sym.pushScope(&objType);
    utl::armed_scope_guard popScope = [&] { sym.popScope(); };
    for (auto* statement: s.body()->statements()) {
        size_t const dependency = dispatch(*statement);
        if (dependency != InvalidIndex) {
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
    SC_ASSERT(varDecl.typeExpr(),
              "In structs variables need explicit type "
              "specifiers. Make this a program issue.");
    auto declResult = sym.declareVariable(std::string(varDecl.name()));
    if (!declResult) {
        iss.push(declResult.error()->setStatement(varDecl));
        return InvalidIndex;
    }
    auto& var = *declResult;
    return dependencyGraph.add({ .entity = &var, .astNode = &varDecl });
}

size_t Context::gather(ast::Statement& statement) {
    using enum InvalidStatement::Reason;
    iss.push<InvalidStatement>(&statement,
                               InvalidScopeForStatement,
                               sym.currentScope());
    return InvalidIndex;
}
