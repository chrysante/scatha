#include "Sema/Analyze.h"

#include "Sema/Analysis/FunctionBodyAnalysis.h"
#include "Sema/Analysis/GatherNames.h"
#include "Sema/Analysis/Instantiation.h"
#include "Sema/Entity.h"

using namespace scatha;
using namespace sema;

AnalysisResult sema::analyze(ast::ASTNode& root,
                             SymbolTable& sym,
                             IssueHandler& iss) {
    AnalysisResult result;
    auto dependencyGraph = gatherNames(sym, root, iss);
    result.structDependencyOrder =
        instantiateEntities(sym, iss, dependencyGraph);
    utl::vector<DependencyGraphNode> functions;
    std::copy_if(dependencyGraph.begin(),
                 dependencyGraph.end(),
                 std::back_inserter(functions),
                 [](DependencyGraphNode const& node) {
        return isa<Function>(node.entity);
    });
    analyzeFunctionBodies(sym, iss, functions);
    return result;
}
