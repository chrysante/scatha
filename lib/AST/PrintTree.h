#ifndef SCATHA_AST_PRINTTREE_H_
#define SCATHA_AST_PRINTTREE_H_

#include <iosfwd>
#include <string>

#include "AST/AST.h"
#include "AST/Expression.h"

namespace scatha::ast {
	
	SCATHA(API) void printTree(AbstractSyntaxTree const*);
	SCATHA(API) void printTree(AbstractSyntaxTree const*, std::ostream&);
	
	std::string toString(Expression const&);

	void printExpression(Expression const&);
	void printExpression(Expression const&, std::ostream&);
	
	
}

#endif // SCATHA_AST_PRINTTREE_H_

