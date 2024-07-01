#include "Sema/Analyze.h"

#include "AST/AST.h"
#include "Sema/Analysis/AnalysisContext.h"
#include "Sema/Analysis/GatherNames.h"
#include "Sema/Analysis/Instantiation.h"
#include "Sema/Analysis/ProtocolConformance.h"
#include "Sema/Analysis/StatementAnalysis.h"
#include "Sema/Entity.h"

using namespace scatha;
using namespace sema;

AnalysisResult sema::analyze(ast::ASTNode& TU, SymbolTable& sym,
                             IssueHandler& iss,
                             AnalysisOptions const& options) {
    sym.setIssueHandler(iss);
    sym.setLibrarySearchPaths(options.librarySearchPaths);
    AnalysisContext ctx(sym, iss);
    auto names = gatherNames(TU, ctx);
    auto structs = instantiateEntities(ctx, names.structs, names.globals);
    for (auto* decl: names.globals) {
        sym.withScopeCurrent(decl->entity()->parent(),
                             [&] { analyzeStatement(ctx, decl); });
    }
    for (auto* recordType: structs) {
        analyzeProtocolConformance(ctx, const_cast<RecordType&>(*recordType));
    }
    return AnalysisResult{ std::move(structs), std::move(names.globals) };
}
