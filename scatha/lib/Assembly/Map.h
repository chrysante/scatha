#ifndef SCATHA_ASSEMBLY_MAP_H_
#define SCATHA_ASSEMBLY_MAP_H_

#include <optional>
#include <utility>

#include <svm/OpCode.h>

#include "Assembly/Common.h"

namespace scatha::Asm {

/// These functions basically perform overload resolution for instructions, i.e.
/// they map instructions and their arguments to opcodes

struct MoveMapResult {
    svm::OpCode opcode;
    size_t numBytes;
};

std::optional<MoveMapResult> mapMove(ValueType dest, ValueType source,
                                     size_t size);

std::optional<MoveMapResult> mapCMove(CompareOperation cmpOp, ValueType dest,
                                      ValueType source, size_t size);

std::optional<svm::OpCode> mapJump(CompareOperation condition);

std::optional<svm::OpCode> mapCall(ValueType destType);

std::optional<svm::OpCode> mapCompare(Type type, ValueType lhs, ValueType rhs,
                                      size_t bitwidth);

std::optional<svm::OpCode> mapTest(Type type, size_t bitwidth);

std::optional<svm::OpCode> mapSet(CompareOperation operation);

std::optional<svm::OpCode> mapArithmetic64(ArithmeticOperation operation,
                                           ValueType dest, ValueType source);

std::optional<svm::OpCode> mapArithmetic32(ArithmeticOperation operation,
                                           ValueType dest, ValueType source);

} // namespace scatha::Asm

#endif // SCATHA_ASSEMBLY_MAP_H_
