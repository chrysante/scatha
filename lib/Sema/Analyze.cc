#include "Sema/Analyze.h"

#include "Sema/Analysis/FunctionAnalysis.h"
#include "Sema/Analysis/GatherNames.h"
#include "Sema/Analysis/Instantiation.h"

using namespace scatha;
using namespace sema;

void sema::analyze(ast::AbstractSyntaxTree& root,
                   SymbolTable& sym,
                   issue::SemaIssueHandler& iss) {
    auto dependencyGraph = gatherNames(sym, root, iss);
    if (iss.fatal()) {
        return;
    }
    instantiateEntities(sym, iss, dependencyGraph);
    if (iss.fatal()) {
        return;
    }
    utl::vector<DependencyGraphNode> functions;
    std::copy_if(dependencyGraph.begin(),
                 dependencyGraph.end(),
                 std::back_inserter(functions),
                 [](DependencyGraphNode const& node) {
        return node.category == SymbolCategory::Function;
    });
    analyzeFunctions(sym, iss, functions);
}
