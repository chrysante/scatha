#ifndef PLAYGROUND_DRAWGRAPH_H_
#define PLAYGROUND_DRAWGRAPH_H_

#include <filesystem>
#include <string>

#include <utl/function_view.hpp>

namespace scatha::ir {

class Module;
class Function;
class BasicBlock;

} // namespace scatha::ir

namespace scatha::mir {

class Function;

} // namespace scatha::mir

namespace scatha::opt {

class SCCCallGraph;

} // namespace scatha::opt

namespace playground {

void drawControlFlowGraph(scatha::ir::Module const& mod,
                          std::filesystem::path const& outFilepath);

std::string drawCallGraph(scatha::opt::SCCCallGraph const& callGraph);

void drawCallGraph(scatha::opt::SCCCallGraph const& callGraph,
                   std::filesystem::path const& outFilepath);

std::string drawInterferenceGraph(scatha::mir::Function const& function);

void drawInterferenceGraph(scatha::mir::Function const& function,
                           std::filesystem::path const& outFilepath);

} // namespace playground

#endif // PLAYGROUND_DRAWGRAPH_H_
