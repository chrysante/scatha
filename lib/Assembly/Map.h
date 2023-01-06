#ifndef SCATHA_ASSEMBLY2_MAP_H_
#define SCATHA_ASSEMBLY2_MAP_H_

#include <utility>

#include "Assembly/Common.h"
#include "VM/OpCode.h"

namespace scatha::Asm {

SCATHA(API) std::pair<vm::OpCode, size_t> mapMove(ValueType dest, ValueType source, size_t size);

SCATHA(API) vm::OpCode mapJump(CompareOperation condition);

SCATHA(API) vm::OpCode mapCompare(Type type, ValueType lhs, ValueType rhs);

SCATHA(API) vm::OpCode mapTest(Type type);

SCATHA(API) vm::OpCode mapSet(CompareOperation operation);

SCATHA(API) vm::OpCode mapArithmetic(ArithmeticOperation operation, Type type, ValueType dest, ValueType source);

} // namespace scatha::Asm

#endif // SCATHA_ASSEMBLY2_MAP_H_
