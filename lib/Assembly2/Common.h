#ifndef SCATHA_ASSEMBLY2_COMMON_H_
#define SCATHA_ASSEMBLY2_COMMON_H_

#include <iosfwd>
#include <string_view>

#include "Basic/Basic.h"

namespace scatha::asm2 {

/// Value types in asm. There are exactly 3 types: signed, unsigned and float
enum class Type {
    Signed, Unsigned, Float, _count
};

/// Forward declare all instructions.

#define SC_ASM_INSTRUCTION_DEF(inst) class inst;
#include "Assembly2/Lists.def"

class Instruction;

/// Enum naming all concrete types in the \p Instruction variant.
enum class InstructionType {
#define SC_ASM_INSTRUCTION_DEF(inst) inst,
#include "Assembly2/Lists.def"
    _count
};

/// Forward declare all values.

#define SC_ASM_VALUE_DEF(value) class value;
#include "Assembly2/Lists.def"

class Value;

/// Enum naming all concrete types in the \p Value variant.
enum class ValueType {
#define SC_ASM_VALUE_DEF(value) value,
#include "Assembly2/Lists.def"
    _count
};

enum class CompareOperation {
#define SC_ASM_COMPARE_DEF(jmpcnd, ...) jmpcnd,
#include "Assembly2/Lists.def"
    _count
};

SCATHA(API) std::string_view toJumpInstName(CompareOperation condition);
SCATHA(API) std::string_view toSetInstName(CompareOperation condition);

enum class ArithmeticOperation {
#define SC_ASM_ARITHMETIC_DEF(op, ...) op,
#include "Assembly2/Lists.def"
    _count
};

SCATHA(API) std::string_view toString(ArithmeticOperation operation);

SCATHA(API) std::ostream& operator<<(std::ostream& ostream, ArithmeticOperation operation);

} // namespace scatha::asm2

#endif // SCATHA_ASSEMBLY2_COMMON_H_

