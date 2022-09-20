#ifndef SCATHA_SEMA_PRINTSYMBOLTABLE_H_
#define SCATHA_SEMA_PRINTSYMBOLTABLE_H_

#include <iosfwd>

#include "Sema/SymbolTable.h"

namespace scatha::sema {
	
	void printSymbolTable(SymbolTable const&);
	
	void printSymbolTable(SymbolTable const&, std::ostream&);
	
}

#endif // SCATHA_SEMA_PRINTSYMBOLTABLE_H_

