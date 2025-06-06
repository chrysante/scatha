#include "Sema/Analysis/GatherNames.h"

#include <utl/vector.hpp>

#include <utl/scope_guard.hpp>

#include "AST/AST.h"
#include "Sema/Analysis/AnalysisContext.h"
#include "Sema/Analysis/ExpressionAnalysis.h"
#include "Sema/Analysis/StatementAnalysis.h"
#include "Sema/Analysis/Utility.h"
#include "Sema/Entity.h"
#include "Sema/SemaIssues.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;
using namespace ranges::views;

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
        globals(result.globals) {}

    size_t gather(ast::ASTNode& node);
    size_t gatherImpl(ast::TranslationUnit&);
    size_t gatherImpl(ast::SourceFile&);
    size_t gatherImpl(ast::ImportStatement&);
    size_t gatherImpl(ast::FunctionDefinition&);
    size_t gatherImpl(ast::RecordDefinition&);
    size_t gatherImpl(ast::BaseClassDeclaration&);
    size_t gatherImpl(ast::VariableDeclaration&);
    size_t gatherImpl(ast::Statement& stmt);
    size_t gatherImpl(ast::ASTNode&) {
        SC_UNREACHABLE(
            "The parser should not allow AST nodes other than statements here");
    }

    AnalysisContext& ctx;
    SymbolTable& sym;
    IssueHandler& iss;
    StructDependencyGraph& dependencyGraph;
    std::vector<ast::Declaration*>& globals;
};

} // namespace

GatherNamesResult scatha::sema::gatherNames(ast::ASTNode& TU,
                                            AnalysisContext& ctx) {
    GatherNamesResult result;
    GatherContext(ctx, result).gather(TU);
    return result;
}

size_t GatherContext::gather(ast::ASTNode& node) {
    return visit(node, [this](auto& node) { return this->gatherImpl(node); });
}

size_t GatherContext::gatherImpl(ast::TranslationUnit& TU) {
    for (auto [index, file]: TU.sourceFiles() | enumerate) {
        auto* scope = sym.declareFileScope(index, file->name());
        sym.withScopeCurrent(scope, [&] { gather(*file); });
    }
    return InvalidIndex;
}

size_t GatherContext::gatherImpl(ast::SourceFile& file) {
    for (auto* stmt: file.statements()) {
        gather(*stmt);
    }
    return InvalidIndex;
}

size_t GatherContext::gatherImpl(ast::ImportStatement& stmt) {
    analyzeStatement(ctx, &stmt);
    return InvalidIndex;
}

size_t GatherContext::gatherImpl(ast::FunctionDefinition& funcDef) {
    if (auto const scopeKind = sym.currentScope().kind();
        scopeKind != ScopeKind::Global && scopeKind != ScopeKind::Namespace &&
        scopeKind != ScopeKind::Type)
    {
        /// Function defintion is only allowed in the global scope, at namespace
        /// scope and structure scope
        ctx.issue<GenericBadStmt>(&funcDef, GenericBadStmt::InvalidScope);
        return InvalidIndex;
    }
    auto* function =
        sym.declareFuncName(&funcDef, determineAccessControl(sym.currentScope(),
                                                             funcDef));
    if (!function) {
        return InvalidIndex;
    }
    funcDef.decorateDecl(function);
    if (funcDef.body()) {
        funcDef.body()->decorateScope(function);
    }
    else {
        if (!funcDef.externalLinkage()) {
            function->markAbstract();
        }
        if (!funcDef.findAncestor<ast::ProtocolDefinition>() &&
            funcDef.externalLinkage() != "C")
        {
            ctx.issue<BadFuncDef>(&funcDef, BadFuncDef::FunctionMustHaveBody);
        }
    }
    /// Now add this function definition to the dependency graph
    globals.push_back(&funcDef);
    return InvalidIndex;
}

size_t GatherContext::gatherImpl(ast::RecordDefinition& def) {
    if (auto const sk = sym.currentScope().kind(); sk != ScopeKind::Global &&
                                                   sk != ScopeKind::Namespace &&
                                                   sk != ScopeKind::Type)
    {
        /// Struct defintion is only allowed in the global scope, at namespace
        /// scope and struct scope
        ctx.issue<GenericBadStmt>(&def, GenericBadStmt::InvalidScope);
        return InvalidIndex;
    }
    auto accessControl = determineAccessControl(sym.currentScope(), def);
    auto* type = sym.declareRecordType(&def, accessControl);
    if (!type) {
        return InvalidIndex;
    }
    def.decorateDecl(type);
    def.body()->decorateScope(type);
    size_t index = dependencyGraph.add({ .entity = type, .astNode = &def });
    /// After we declared this type we gather all its members
    sym.withScopePushed(type, [&] {
        for (auto* statement:
             concat(def.baseClasses(), def.body()->statements()))
        {
            size_t dependency = gather(*statement);
            if (dependency != InvalidIndex) {
                dependencyGraph[index].dependencies.push_back(
                    utl::narrow_cast<u16>(dependency));
            }
        }
    });
    return index;
}

size_t GatherContext::gatherImpl(ast::BaseClassDeclaration& baseDecl) {
    SC_ASSERT(isa<RecordType>(sym.currentScope()), "");
    if (!baseDecl.typeExpr()) {
        return InvalidIndex;
    }
    auto* object =
        sym.declareBaseClass(&baseDecl,
                             determineAccessControl(sym.currentScope(),
                                                    baseDecl));
    if (!object) {
        return InvalidIndex;
    }
    return dependencyGraph.add({ .entity = object, .astNode = &baseDecl });
}

size_t GatherContext::gatherImpl(ast::VariableDeclaration& varDecl) {
    SC_ASSERT(isa<RecordType>(sym.currentScope()) ||
                  isa<FileScope>(sym.currentScope()),
              "Local variables will be analyzed later");
    if (!varDecl.typeExpr()) {
        ctx.issue<BadVarDecl>(&varDecl, BadVarDecl::GlobalNeedsTypeSpecifier);
        return InvalidIndex;
    }
    auto* variable =
        sym.declareVariable(&varDecl, determineAccessControl(sym.currentScope(),
                                                             varDecl));
    if (!variable) {
        return InvalidIndex;
    }
    if (variable->isStatic()) {
        varDecl.decorateVarDecl(variable);
        globals.push_back(&varDecl);
        return InvalidIndex;
    }
    else {
        return dependencyGraph.add({ .entity = variable, .astNode = &varDecl });
    }
}

size_t GatherContext::gatherImpl(ast::Statement& stmt) {
    ctx.issue<GenericBadStmt>(&stmt, GenericBadStmt::InvalidScope);
    return InvalidIndex;
}
