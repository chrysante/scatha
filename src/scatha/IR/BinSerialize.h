#ifndef SCATHA_IR_BINSERIALIZE_H_
#define SCATHA_IR_BINSERIALIZE_H_

#include <iosfwd>

#include "IR/Fwd.h"

namespace scatha::ir {

/// Writes a binary representation of the contents of the IR module \p mod to
/// the ostream \p out
void serializeBinary(ir::Module const& mod, std::ostream& out);

/// Parses the binary module representation in the istream \p in into the module
/// \p mod
bool deserializeBinary(ir::Context& ctx, ir::Module& mod, std::istream& in);

} // namespace scatha::ir

#endif // SCATHA_IR_BINSERIALIZE_H_
