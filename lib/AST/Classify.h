#ifndef SCATHA_AST_CLASSIFY_H_
#define SCATHA_AST_CLASSIFY_H_

#include "AST/Common.h"

namespace scatha::ast {
	
bool isDeclaration(NodeType);
	
BinaryOperator toNonAssignment(BinaryOperator);

bool isArithmetic(BinaryOperator);

bool isLogical(BinaryOperator);

bool isComparison(BinaryOperator);

bool isAssignment(BinaryOperator);

} // namespace scatha::ast

#endif // SCATHA_AST_CLASSIFY_H_

