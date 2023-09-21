#include "Sema/Analyze.h"

#include "Sema/Analysis/FunctionBodyAnalysis.h"
#include "Sema/Analysis/GatherNames.h"
#include "Sema/Analysis/Instantiation.h"
#include "Sema/Context.h"
#include "Sema/Entity.h"

using namespace scatha;
using namespace sema;

AnalysisResult sema::analyze(ast::ASTNode& root,
                             SymbolTable& sym,
                             IssueHandler& iss) {
    Context ctx(sym, iss);
    auto names = gatherNames(root, ctx);
    auto structs = instantiateEntities(ctx, names.structs, names.functions);
    for (auto* def: names.functions) {
        analyzeFunction(ctx, def);
    }
    return AnalysisResult{ std::move(structs), std::move(names.functions) };
}
