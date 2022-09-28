#ifndef SCATHA_AST_PRINTSOURCE_H_
#define SCATHA_AST_PRINTSOURCE_H_

#include <iosfwd>

#include "AST/AST.h"
#include "Basic/Basic.h"

namespace scatha::ast {
	
	SCATHA(API) void printSource(AbstractSyntaxTree const*);
	SCATHA(API) void printSource(AbstractSyntaxTree const*, std::ostream&);
	
}

#endif // SCATHA_AST_PRINTSOURCE_H_

