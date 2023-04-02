#ifndef SCATHA_ASSEMBLY_MAP_H_
#define SCATHA_ASSEMBLY_MAP_H_

#include <utility>

#include <svm/OpCode.h>

#include "Assembly/Common.h"

namespace scatha::Asm {

std::pair<svm::OpCode, size_t> mapMove(ValueType dest,
                                       ValueType source,
                                       size_t size);

std::pair<svm::OpCode, size_t> mapCMove(CompareOperation cmpOp,
                                        ValueType dest,
                                        ValueType source,
                                        size_t size);

svm::OpCode mapJump(CompareOperation condition);

svm::OpCode mapCompare(Type type,
                       ValueType lhs,
                       ValueType rhs,
                       size_t bitwidth);

svm::OpCode mapTest(Type type, size_t bitwidth);

svm::OpCode mapSet(CompareOperation operation);

svm::OpCode mapArithmetic64(ArithmeticOperation operation,
                            ValueType dest,
                            ValueType source);

svm::OpCode mapArithmetic32(ArithmeticOperation operation,
                            ValueType dest,
                            ValueType source);

} // namespace scatha::Asm

#endif // SCATHA_ASSEMBLY_MAP_H_
