#include "Sema/Analyze.h"

#include "Sema/Analysis/GatherNames.h"
#include "Sema/Analysis/Instantiation.h"
#include "Sema/Analysis/FunctionAnalysis.h"

using namespace scatha;
using namespace sema;

SymbolTable sema::analyze(ast::AbstractSyntaxTree* root, issue::IssueHandler& iss) {
    SymbolTable sym;
    auto dependencyGraph = gatherNames(sym, *root, iss);
    if (iss.fatal()) { return sym; }
    instantiateEntities(sym, iss, dependencyGraph);
    if (iss.fatal()) { return sym; }
    analyzeFunctions(sym, iss, dependencyGraph);
    return sym;
}
