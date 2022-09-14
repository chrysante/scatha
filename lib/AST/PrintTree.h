#ifndef SCATHA_AST_PRINTTREE_H_
#define SCATHA_AST_PRINTTREE_H_

#include <iosfwd>

#include "AST/AST.h"

namespace scatha::ast {
	
	void printTree(AbstractSyntaxTree const*);
	void printTree(AbstractSyntaxTree const*, std::ostream&);
	
}

#endif // SCATHA_AST_PRINTTREE_H_

