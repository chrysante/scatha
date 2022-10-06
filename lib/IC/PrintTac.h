#ifndef SCATHA_IC_PRINTTAC_H_
#define SCATHA_IC_PRINTTAC_H_

#include <iosfwd>

#include "IC/ThreeAddressCode.h"
#include "Sema/SymbolTable.h"

namespace scatha::ic {

void SCATHA(API) printTac(ThreeAddressCode const &, sema::SymbolTable const &);
void SCATHA(API) printTac(ThreeAddressCode const &, sema::SymbolTable const &, std::ostream &);

} // namespace scatha::ic

#endif // SCATHA_IC_PRINTTAC_H_
