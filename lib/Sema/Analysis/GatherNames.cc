#include "Sema/Analysis/GatherNames.h"

#include <utl/vector.hpp>

#include <utl/scope_guard.hpp>

#include "AST/AST.h"
#include "Sema/Analysis/AnalysisContext.h"
#include "Sema/Analysis/ExpressionAnalysis.h"
#include "Sema/Entity.h"
#include "Sema/SemanticIssue.h"
#include "Sema/SemanticIssuesNEW.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace scatha::sema;

static constexpr size_t InvalidIndex = ~size_t{};

namespace {

/// Gathers all declarations and declares them in the symbol table. Also
/// analyzes the dependencies of structs because they are trivial.
struct GatherContext {
    GatherContext(AnalysisContext& ctx, GatherNamesResult& result):
        ctx(ctx),
        sym(ctx.symbolTable()),
        iss(ctx.issueHandler()),
        dependencyGraph(result.structs),
        functions(result.functions) {}

    /// Dispatches to the appropriate one of the `gatherImpl()` overloads below
    /// based on the runtime type of \p node
    size_t gather(ast::ASTNode& node);

    size_t gatherImpl(ast::TranslationUnit&);
    size_t gatherImpl(ast::FunctionDefinition&);
    size_t gatherImpl(ast::StructDefinition&);
    size_t gatherImpl(ast::VariableDeclaration&);
    size_t gatherImpl(ast::Statement&);
    size_t gatherImpl(ast::ASTNode&) { SC_UNREACHABLE(); }

    AnalysisContext& ctx;
    SymbolTable& sym;
    IssueHandler& iss;
    StructDependencyGraph& dependencyGraph;
    utl::vector<ast::FunctionDefinition*>& functions;
};

} // namespace

GatherNamesResult scatha::sema::gatherNames(ast::ASTNode& root,
                                            AnalysisContext& ctx) {
    GatherNamesResult result;
    GatherContext(ctx, result).gather(root);
    return result;
}

size_t GatherContext::gather(ast::ASTNode& node) {
    return visit(node, [this](auto& node) { return this->gatherImpl(node); });
}

size_t GatherContext::gatherImpl(ast::TranslationUnit& tu) {
    for (auto* decl: tu.declarations()) {
        gather(*decl);
    }
    return InvalidIndex;
}

size_t GatherContext::gatherImpl(ast::FunctionDefinition& funcDef) {
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
    auto* function = sym.declareFuncName(&funcDef);
    if (!function) {
        return InvalidIndex;
    }
    function->setASTNode(&funcDef);
    funcDef.decorateDecl(function);
    funcDef.body()->decorateScope(function);
    /// Now add this function definition to the dependency graph
    functions.push_back(&funcDef);
    return InvalidIndex;
}

size_t GatherContext::gatherImpl(ast::StructDefinition& def) {
    if (auto const sk = sym.currentScope().kind(); sk != ScopeKind::Global &&
                                                   sk != ScopeKind::Namespace &&
                                                   sk != ScopeKind::Object)
    {
        /// Struct defintion is only allowed in the global scope, at namespace
        /// scope and struct scope
        ctx.issue<GenericBadDecl>(&def, GenericBadDecl::InvalidInScope);
        return InvalidIndex;
    }
    auto* type = sym.declareStructureType(&def);
    if (!type) {
        return InvalidIndex;
    }
    type->setASTNode(&def);
    def.decorateDecl(type);
    def.body()->decorateScope(type);
    size_t const index =
        dependencyGraph.add({ .entity = type, .astNode = &def });
    /// After we declared this type we gather all its members
    sym.withScopePushed(type, [&] {
        for (auto* statement: def.body()->statements()) {
            size_t const dependency = gather(*statement);
            if (dependency != InvalidIndex) {
                dependencyGraph[index].dependencies.push_back(
                    utl::narrow_cast<u16>(dependency));
            }
        }
    });
    return index;
}

size_t GatherContext::gatherImpl(ast::VariableDeclaration& varDecl) {
    SC_ASSERT(sym.currentScope().kind() == ScopeKind::Object,
              "We only want to prepass struct definitions. What are we doing "
              "here?");
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

size_t GatherContext::gatherImpl(ast::Statement& statement) {
    using enum InvalidStatement::Reason;
    iss.push<InvalidStatement>(&statement,
                               InvalidScopeForStatement,
                               sym.currentScope());
    return InvalidIndex;
}
