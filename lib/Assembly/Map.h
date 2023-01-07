#ifndef SCATHA_ASSEMBLY_MAP_H_
#define SCATHA_ASSEMBLY_MAP_H_

#include <utility>

#include "Assembly/Common.h"
#include "VM/OpCode.h"

namespace scatha::Asm {

std::pair<vm::OpCode, size_t> mapMove(ValueType dest, ValueType source, size_t size);

vm::OpCode mapJump(CompareOperation condition);

vm::OpCode mapCompare(Type type, ValueType lhs, ValueType rhs);

vm::OpCode mapTest(Type type);

vm::OpCode mapSet(CompareOperation operation);

vm::OpCode mapArithmetic(ArithmeticOperation operation, Type type, ValueType dest, ValueType source);

} // namespace scatha::Asm

#endif // SCATHA_ASSEMBLY_MAP_H_
