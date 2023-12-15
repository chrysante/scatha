#include "Sema/Analyze.h"

#include "AST/AST.h"
#include "Sema/Analysis/AnalysisContext.h"
#include "Sema/Analysis/FunctionAnalysis.h"
#include "Sema/Analysis/GatherNames.h"
#include "Sema/Analysis/Instantiation.h"
#include "Sema/Entity.h"

using namespace scatha;
using namespace sema;

AnalysisResult sema::analyze(ast::ASTNode& TU,
                             SymbolTable& sym,
                             IssueHandler& iss,
                             AnalysisOptions const& options) {
    sym.setIssueHandler(iss);
    AnalysisContext ctx(sym, iss);
    auto names = gatherNames(TU, ctx, options.librarySearchPaths);
    auto structs = instantiateEntities(ctx, names.structs, names.functions);
    for (auto* def: names.functions) {
        analyzeFunction(ctx, def);
    }
    return AnalysisResult{ std::move(structs), std::move(names.functions) };
}
