#ifndef SCATHA_SEMA_PRINT_H_
#define SCATHA_SEMA_PRINT_H_

#include <iosfwd>

#include <scatha/Sema/Fwd.h>

namespace scatha::sema {

/// Options passed to `print(SymbolTable const&)`
struct PrintOptions {
    /// Set to true to print builtin types and functions
    bool printBuiltins;
};

/// Print \p symbolTable to `std::cout` with default options
SCATHA_API void print(SymbolTable const& symbolTable);

/// Print \p symbolTable to \p ostream
SCATHA_API void print(SymbolTable const& symbolTable, std::ostream& ostream,
                      PrintOptions const& options = {});

} // namespace scatha::sema

#endif // SCATHA_SEMA_PRINT_H_
