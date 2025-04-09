#ifndef SCATHA_IR_GRAPHVIZ_H_
#define SCATHA_IR_GRAPHVIZ_H_

#include <iosfwd>

#include <scatha/IR/Fwd.h>

namespace scatha::ir {

/// Argument structure for `generateGraphviz()`
struct GraphvizArgs {
    /// Highlight loops in the generated image
    bool markLoops = false;
};

/// Generates graphviz source for a visual representation of the function \p
/// function and writes it into \p ostream
SCATHA_API void generateGraphviz(Function const& function, GraphvizArgs args,
                                 std::ostream& ostream);

/// Generates graphviz source for a visual representation of the module \p mod
/// and writes it into \p ostream
SCATHA_API void generateGraphviz(Module const& mod, GraphvizArgs args,
                                 std::ostream& ostream);

/// Debug utility to generate graphical CFG representation of the function to a
/// temporary file
SCATHA_API void generateGraphvizTmp(Function const& function);

/// Debug utility to generate graphical CFG representation of the module to a
/// temporary file
SCATHA_API void generateGraphvizTmp(Module const& mod);

} // namespace scatha::ir

#endif // SCATHA_IR_GRAPHVIZ_H_
