#ifndef SCATHA_OPT_PATHFINDER_H_
#define SCATHA_OPT_PATHFINDER_H_

#include <utl/vector.hpp>

#include "Basic/Basic.h"
#include "IR/Common.h"

namespace scatha::opt {

class ControlFlowPath;

/// Find all the paths in the control flow graphs from \p origin to \p dest
/// \pre \p origin and \p dest must be in the same function.
SCATHA(TEST_API) utl::vector<ControlFlowPath> findAllPaths(ir::Instruction const* a, ir::Instruction const* b);

} // namespace scatha::opt

#endif // SCATHA_OPT_PATHFINDER_H_
