#ifndef SVM_OPCODE_H_
#define SVM_OPCODE_H_

#include <array>
#include <iosfwd>
#include <string_view>

#include <utl/utility.hpp>
#include <utl/vector.hpp>

#include <svm/Common.h>

namespace svm {

/// ## A program looks like this:
/// ```
/// u8 [instruction], u8... [arguments]
/// ...
/// ```
///
/// ```
/// MEMORY_POINTER         == [u8 baseptrRegIdx,
///                            u8 offsetCountRegIdx,
///                            u8 constantOffsetMultiplier,
///                            u8 constantInnerOffset]
/// eval(MEMORY_POINTER)   == reg[baseptrRegIdx] + offsetCountRegIdx *
/// constantOffsetMultiplier + constantInnerOffset sizeof(MEMORY_POINTER) == 4
/// ```
/// NOTE: If `offsetCountRegIdx == 0xFF` then `eval(MEMORY_POINTER) ==
/// reg[baseptrRegIdx] + constantInnerOffset`

/// ## Calling convention
///
/// _All register indices are from the perspective of the callee_
///
/// Arguments are passed in consecutive registers starting with index 0.
/// Return value is passed in consecutive registers starting with index 0.
/// All registers with positive indices may be used and modified by the callee.
/// The register pointer offset is placed in `R[-2]` and added to the
/// register pointer by the `call` instruction. The register pointer offset is
/// subtracted from the register pointer by the the `ret` instruction. The
/// return address is placed in `R[-1]` by the `call` instruction.
///

/// Opcodes are stored as 12 bit integers. The next 4 bits encode the offset to
/// the next instruction.
enum class OpCode : u8 {
#define SVM_INSTRUCTION_DEF(inst, class) inst,
#include <svm/Lists.def>
    _count
};

std::string_view toString(OpCode);

std::ostream& operator<<(std::ostream&, OpCode);

enum class OpCodeClass { RR, RV64, RV32, RV8, RM, MR, R, Jump, Other, _count };

/// Maps opcodes to their class
inline constexpr OpCodeClass classify(OpCode code) {
    return std::array{
#define SVM_INSTRUCTION_DEF(inst, class) OpCodeClass::class,
#include <svm/Lists.def>
    }[static_cast<size_t>(code)];
};

/// \Returns The offset in bytes to the next instruction.
inline constexpr size_t codeSize(OpCode code) {
    using enum OpCodeClass;
    auto const opCodeClass = classify(code);
    if (opCodeClass == Other) {
        switch (code) {
        case OpCode::call:
            return sizeof(OpCode) + 4 + 1;
        case OpCode::ret:
            return sizeof(OpCode);
        case OpCode::terminate:
            return sizeof(OpCode);
        case OpCode::callExt:
            return sizeof(OpCode) + 1 + 1 + 2;
        case OpCode::lincsp:
            return sizeof(OpCode) + 1 + 2;
        default:
            assert(false);
        }
    }
    // clang-format off
    return UTL_MAP_ENUM(opCodeClass, size_t, {
        { OpCodeClass::RR,     sizeof(OpCode) + 1 + 1 },
        { OpCodeClass::RV64,   sizeof(OpCode) + 1 + 8 },
        { OpCodeClass::RV32,   sizeof(OpCode) + 1 + 4 },
        { OpCodeClass::RV8,    sizeof(OpCode) + 1 + 1 },
        { OpCodeClass::RM,     sizeof(OpCode) + 1 + 4 },
        { OpCodeClass::MR,     sizeof(OpCode) + 4 + 1 },
        { OpCodeClass::R,      sizeof(OpCode) + 1 },
        { OpCodeClass::Jump,   sizeof(OpCode) + 4 },
        { OpCodeClass::Other, static_cast<size_t>(-1) }
    });
    // clang-format on
}

} // namespace svm

#endif // SVM_OPCODE_H_
