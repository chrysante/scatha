#include "Sema/Analyze.h"

#include "Sema/Analysis/FunctionAnalysis.h"
#include "Sema/Analysis/GatherNames.h"
#include "Sema/Analysis/Instantiation.h"

using namespace scatha;
using namespace sema;

SymbolTable sema::analyze(ast::AbstractSyntaxTree& root, issue::SemaIssueHandler& iss) {
    SymbolTable sym;
    auto dependencyGraph = gatherNames(sym, root, iss);
    if (iss.fatal()) {
        return sym;
    }
    instantiateEntities(sym, iss, dependencyGraph);
    if (iss.fatal()) {
        return sym;
    }
    utl::vector<DependencyGraphNode> functions;
    std::copy_if(dependencyGraph.begin(),
                 dependencyGraph.end(),
                 std::back_inserter(functions),
                 [](DependencyGraphNode const& node) { return node.category == SymbolCategory::Function; });
    analyzeFunctions(sym, iss, functions);
    return sym;
}
