#ifndef SCATHA_IR_GRAPHVIZ_H_
#define SCATHA_IR_GRAPHVIZ_H_

#include <iosfwd>

#include "IR/Fwd.h"

namespace scatha::ir {

/// Generates graphviz source for a visual representation of the function \p
/// function and writes it into \p ostream
SCATHA_API void generateGraphviz(ir::Function const& function,
                                 std::ostream& ostream);

/// Generates graphviz source for a visual representation of the module \p mod
/// and writes it into \p ostream
SCATHA_API void generateGraphviz(ir::Module const& mod, std::ostream& ostream);

} // namespace scatha::ir

#endif // SCATHA_IR_GRAPHVIZ_H_
