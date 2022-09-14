#ifndef SCATHA_AST_PRINTSOURCE_H_
#define SCATHA_AST_PRINTSOURCE_H_

#include <iosfwd>

#include "AST/AST.h"

namespace scatha::ast {
	
	void printSource(AbstractSyntaxTree const*);
	void printSource(AbstractSyntaxTree const*, std::ostream&);
	
}

#endif // SCATHA_AST_PRINTSOURCE_H_

