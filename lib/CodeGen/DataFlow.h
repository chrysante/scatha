#ifndef SCATHA_CODEGEN_DATAFLOW_H_
#define SCATHA_CODEGEN_DATAFLOW_H_

#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include "Basic/Basic.h"
#include "MIR/Fwd.h"

namespace scatha::cg {

/// Computes the live-in and live-out sets for each basic block of function
/// \p F
SCATHA_TESTAPI void computeLiveSets(mir::Function& F);

} // namespace scatha::cg

#endif // SCATHA_CODEGEN_DATAFLOW_H_
