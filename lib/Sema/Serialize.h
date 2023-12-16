#ifndef SCATHA_SEMA_SERIALIZE_H_
#define SCATHA_SEMA_SERIALIZE_H_

#include <iosfwd>

#include "Common/Base.h"
#include "Sema/Fwd.h"

namespace scatha::sema {

/// Writes public declarations in the global scope in \p sym in JSON format to
/// \p ostream
void SCATHA_API serialize(SymbolTable const& sym, std::ostream& ostream);

/// Parses declared symbols in JSON format in \p ostream and declares the parsed
/// symbols to the current scope of \p sym
bool SCATHA_API deserialize(SymbolTable& sym, std::istream& istream);

} // namespace scatha::sema

#endif // SCATHA_SEMA_SERIALIZE_H_
