#ifndef SCATHA_ASSEMBLY_COMMON_H_
#define SCATHA_ASSEMBLY_COMMON_H_

#include <iosfwd>
#include <string_view>

#include "Common/Base.h"

namespace scatha::Asm {

/// Strong integer typedef to disambiguate label IDs from other integers
enum class LabelID : u64 {};

/// A position in the code from where a jump occurs or where a label address is
/// stored
struct Jumpsite {
    /// The position where the dest address will be stored to
    size_t codePosition;

    /// The ID of the dest address
    LabelID targetID;

    /// The width with which we store the dest address.
    size_t width;
};

/// Value types in asm. There are exactly 3 types: signed, unsigned and float
enum class Type : u8 { Signed, Unsigned, Float, _count };

/// Forward declare all instructions.

#define SC_ASM_INSTRUCTION_DEF(inst) class inst;
#include "Assembly/Lists.def.h"

class Instruction;

/// Enum naming all concrete types in the `Instruction` variant.
enum class InstructionType {
#define SC_ASM_INSTRUCTION_DEF(inst) inst,
#include "Assembly/Lists.def.h"
    _count
};

/// Forward declare all values.

#define SC_ASM_VALUE_DEF(value) class value;
#include "Assembly/Lists.def.h"

class Value;

/// Enum naming all concrete types in the `Value` variant.
enum class ValueType {
#define SC_ASM_VALUE_DEF(value) value,
#include "Assembly/Lists.def.h"
    _count
};

size_t sizeOf(ValueType type);

bool isLiteralValue(ValueType type);

ValueType promote(ValueType type, size_t size);

enum class CompareOperation {
#define SC_ASM_COMPARE_DEF(jmpcnd, ...) jmpcnd,
#include "Assembly/Lists.def.h"
    _count
};

std::string_view toCMoveInstName(CompareOperation condition);
std::string_view toJumpInstName(CompareOperation condition);
std::string_view toSetInstName(CompareOperation condition);

enum class UnaryArithmeticOperation {
#define SC_ASM_UNARY_ARITHMETIC_DEF(op, ...) op,
#include "Assembly/Lists.def.h"
    _count
};

SCATHA_API std::string_view toString(UnaryArithmeticOperation operation);

SCATHA_API std::ostream& operator<<(std::ostream& ostream,
                                    UnaryArithmeticOperation operation);

enum class ArithmeticOperation {
#define SC_ASM_ARITHMETIC_DEF(op, ...) op,
#include "Assembly/Lists.def.h"
    _count
};

SCATHA_API std::string_view toString(ArithmeticOperation operation);

SCATHA_API std::ostream& operator<<(std::ostream& ostream,
                                    ArithmeticOperation operation);

/// \Returns `true` if operation \p op is a (logical or arithmetic) shift
/// operation.
bool isShift(ArithmeticOperation op);

} // namespace scatha::Asm

#endif // SCATHA_ASSEMBLY_COMMON_H_
