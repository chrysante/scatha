#ifndef SVM_OPCODEINTERNAL_H_
#define SVM_OPCODEINTERNAL_H_

#include <cassert>

#include <utl/utility.hpp>

#include <svm/OpCode.h>

namespace svm {

enum class OpCodeClass { RR, RV64, RV32, RV8, RM, MR, R, Jump, Other, _count };

/// Maps opcodes to their class
inline constexpr OpCodeClass classify(OpCode code) {
    return std::array{
#define SVM_INSTRUCTION_DEF(inst, class) OpCodeClass::class,
#include <svm/OpCode.def>
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

#endif // SVM_OPCODEINTERNAL_H_
