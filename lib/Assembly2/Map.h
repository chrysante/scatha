#ifndef SCATHA_ASSEMBLY2_MAP_H_
#define SCATHA_ASSEMBLY2_MAP_H_

#include "Assembly2/Common.h"
#include "VM/OpCode.h"

namespace scatha::asm2 {

SCATHA(API) vm::OpCode mapMove(ElemType dest, ElemType source);

SCATHA(API) vm::OpCode mapJump(CompareOperation condition);

SCATHA(API) vm::OpCode mapCompare(Type type, ElemType lhs, ElemType rhs);

SCATHA(API) vm::OpCode mapTest(Type type);

SCATHA(API) vm::OpCode mapSet(CompareOperation operation);

SCATHA(API) vm::OpCode mapArithmetic(ArithmeticOperation operation, Type type, ElemType lhs, ElemType rhs);

} // namespace scatha::asm2

#endif // SCATHA_ASSEMBLY2_MAP_H_

