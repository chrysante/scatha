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
    auto names = gatherNames(sym, root, iss);
    auto structs =
        instantiateEntities(sym, iss, names.structs, names.functions);
    analyzeFunctionBodies(sym, iss, names.functions);
    return AnalysisResult{ std::move(structs) };
}
