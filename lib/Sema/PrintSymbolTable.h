#ifndef SCATHA_SEMA_PRINTSYMBOLTABLE_H_
#define SCATHA_SEMA_PRINTSYMBOLTABLE_H_

#include <iosfwd>

#include "Sema/SymbolTable.h"

namespace scatha::sema {
	
	void SCATHA(API) printSymbolTable(SymbolTable const&);
	
	void SCATHA(API) printSymbolTable(SymbolTable const&, std::ostream&);

	std::string makeQualName(EntityBase const&);
	
}

#endif // SCATHA_SEMA_PRINTSYMBOLTABLE_H_

