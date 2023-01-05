#ifndef SCATHA_VM_INSTRUCTION_H_
#define SCATHA_VM_INSTRUCTION_H_

#include <array>
#include <iosfwd>
#include <string_view>

#include <utl/utility.hpp>
#include <utl/vector.hpp>

#include "Basic/Basic.h"

namespace scatha::vm {

/// ** A program looks like this: **
// u8 [instruction], u8... [arguments]
// ...
///
// MEMORY_POINTER           ==   u8 ptrRegIdx, u8 offset, u8 offsetShift
// eval(MEMORY_POINTER)     ==   reg[ptrRegIdx] + (offset << offsetShift)
// sizeof(MEMORY_POINTER)   ==   3

/// ** Calling convention **
/// _ All register indices are from the position of the callee _
///
/// Arguments are passed in consecutive registers starting with index 0.
/// Return value is passed in consecutive registers starting with index 0.
/// All registers with positive indices may be used and modified by the callee.
/// The register  pointer offset is placed in \p R[-2] and added to the
/// register pointer by the \p call instruction. The register pointer offset is
/// subtracted from the register pointer by the the \p ret instruction. The
/// return address is placed in \p R[-1] by the \p call instruction.
///

enum class OpCode: u8 {
#define SC_INSTRUCTION_DEF(inst, class) inst,
#include "VM/Lists.def"
    _count
};

SCATHA(API) std::string_view toString(OpCode);

SCATHA(API) std::ostream& operator<<(std::ostream&, OpCode);

enum class OpCodeClass { RR, RV, RM, MR, R, Jump, Other, _count };

constexpr bool isJump(OpCode c) {
    using enum OpCode;
    u8 const cRawValue = static_cast<u8>(c);
    return (cRawValue >= static_cast<u8>(jmp) && cRawValue <= static_cast<u8>(jge)) || c == call;
}

constexpr OpCodeClass classify(OpCode c) {
    return std::array{
#define SC_INSTRUCTION_DEF(inst, class) OpCodeClass::class,
#include "VM/Lists.def"
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
        default: SC_UNREACHABLE();
        }
    }
    return UTL_MAP_ENUM(opCodeClass, size_t, {
        { OpCodeClass::RR,     3 },
        { OpCodeClass::RV,    10 },
        { OpCodeClass::RM,     5 },
        { OpCodeClass::MR,     5 },
        { OpCodeClass::R,      2 },
        { OpCodeClass::Jump,   5 },
        { OpCodeClass::Other, static_cast<size_t>(-1) }
    });
    // clang-format on
}

using Instruction = u64 (*)(u8 const*, u64*, class VirtualMachine*);

utl::vector<Instruction> makeInstructionTable();

} // namespace scatha::vm

#endif // SCATHA_VM_INSTRUCTION_H_
