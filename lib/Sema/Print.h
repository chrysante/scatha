#ifndef SCATHA_SEMA_PRINT_H_
#define SCATHA_SEMA_PRINT_H_

#include <iosfwd>

#include "Sema/SymbolTable.h"

namespace scatha::sema {

void SCATHA_API printSymbolTable(SymbolTable const&);

void SCATHA_API printSymbolTable(SymbolTable const&, std::ostream&);

std::string makeQualName(EntityBase const&);

} // namespace scatha::sema

#endif // SCATHA_SEMA_PRINT_H_
