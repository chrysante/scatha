#ifndef SVM_OPCODE_H_
#define SVM_OPCODE_H_

#include <array>
#include <iosfwd>
#include <string_view>

#include <utl/utility.hpp>
#include <utl/vector.hpp>

#include <svm/Common.h>
#include <svm/Instruction.h>

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

enum class OpCode : u8 {
#define SVM_INSTRUCTION_DEF(inst, class) inst,
#include <svm/Lists.def>
    _count
};

std::string_view toString(OpCode);

std::ostream& operator<<(std::ostream&, OpCode);

enum class OpCodeClass { RR, RV, RM, MR, R, Jump, Other, _count };

constexpr bool isJump(OpCode c) {
    using enum OpCode;
    u8 const cRawValue = static_cast<u8>(c);
    return (cRawValue >= static_cast<u8>(jmp) &&
            cRawValue <= static_cast<u8>(jge)) ||
           c == call;
}

constexpr OpCodeClass classify(OpCode c) {
    return std::array{
#define SVM_INSTRUCTION_DEF(inst, class) OpCodeClass::class,
#include <svm/Lists.def>
    }[static_cast<size_t>(c)];
}

constexpr size_t codeSize(OpCode c) {
    // clang-format off
    using enum OpCodeClass;
    auto const opCodeClass = classify(c);
    if (opCodeClass == Other) {
        switch (c) {
        case OpCode::call:            return 6;
        case OpCode::ret:             return 1;
        case OpCode::terminate:       return 1;
        case OpCode::callExt:         return 5;
        case OpCode::alloca_:         return 3;
        case OpCode::incsp:           return 3;
        case OpCode::decsp:           return 3;
        case OpCode::loadsp:          return 2;
        case OpCode::storesp:         return 2;
        default: assert(false);
        }
    }
    return UTL_MAP_ENUM(opCodeClass, size_t, {
        { OpCodeClass::RR,     3 },
        { OpCodeClass::RV,    10 },
        { OpCodeClass::RM,     6 },
        { OpCodeClass::MR,     6 },
        { OpCodeClass::R,      2 },
        { OpCodeClass::Jump,   5 },
        { OpCodeClass::Other, static_cast<size_t>(-1) }
    });
    // clang-format on
}

utl::vector<Instruction> makeInstructionTable();

} // namespace svm

#endif // SVM_OPCODE_H_
