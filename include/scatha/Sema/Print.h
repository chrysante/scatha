#ifndef SCATHA_SEMA_PRINT_H_
#define SCATHA_SEMA_PRINT_H_

#include <iosfwd>

#include <scatha/Sema/Fwd.h>

namespace scatha::sema {

/// Print \p symbolTable to `std::cout`
SCATHA_API void print(SymbolTable const& symbolTable);

/// Print \p symbolTable to \p ostream
SCATHA_API void print(SymbolTable const& symbolTable, std::ostream& ostream);

} // namespace scatha::sema

#endif // SCATHA_SEMA_PRINT_H_
