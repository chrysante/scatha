#ifndef SCATHA_SEMANTICANALYZER_PRINTSYMBOLTABLE_H_
#define SCATHA_SEMANTICANALYZER_PRINTSYMBOLTABLE_H_

#include <iosfwd>

#include "SemanticAnalyzer/SymbolTable.h"

namespace scatha::sem {
	
	void printSymbolTable(SymbolTable const&);
	
	void printSymbolTable(SymbolTable const&, std::ostream&);
	
}

#endif // SCATHA_SEMANTICANALYZER_PRINTSYMBOLTABLE_H_

