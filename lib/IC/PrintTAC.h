#ifndef SCATHA_IC_PRINTTAC_H_
#define SCATHA_IC_PRINTTAC_H_

#include <iosfwd>

#include "IC/TAS.h"
#include "Sema/SymbolTable.h"

namespace scatha::ic {
	
	void printTAC(TAC const&, sema::SymbolTable const&);
	void printTAC(TAC const&, sema::SymbolTable const&, std::ostream&);
	
}

#endif // SCATHA_IC_PRINTTAC_H_

