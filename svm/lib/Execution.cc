#include "VirtualMachine.h"

#include <cassert>

#include <utl/functional.hpp>

#include "Common.h"
#include "Memory.h"
#include "OpCode.h"
#include "VMImpl.h"

using namespace svm;

/// \Returns `codeSize(code)`  except for call and terminate instruction for
/// which this function returns 0. This is used to advance the instruction
/// pointer. Since these three instructions alter the instruction pointer we do
/// not want to advance it any further.
/// Jump instructions subtract the codesize from the target because we have
/// conditional jumps and advance the instruction pointer unconditionally
static constexpr size_t execCodeSizeImpl(OpCode code) {
    if (code == OpCode::call) {
        return 0;
    }
    if (code == OpCode::icallr) {
        return 0;
    }
    if (code == OpCode::icallm) {
        return 0;
    }
    if (code == OpCode::ret) {
        return 0;
    }
    if (code == OpCode::terminate) {
        return 0;
    }
    return codeSize(code);
}

/// Making this a template actually cuts execution time in debug builds in half
/// compared to calling `execCodeSizeImpl()` directly.
template <OpCode Code>
static constexpr size_t ExecCodeSize = execCodeSizeImpl(Code);

template <OpCode Code>
static constexpr size_t CodeSize = codeSize(Code);

template <typename T>
static void storeReg(u64* dest, T const& t) {
    std::memset(dest, 0, 8);
    std::memcpy(dest, &t, sizeof(T));
}

static VirtualPointer getPointer(u64 const* reg, u8 const* i) {
    uint64_t baseptrRegIdx = i[0];
    uint64_t offsetCountRegIdx = i[1];
    i64 constantOffsetMultiplier = i[2];
    i64 constantInnerOffset = i[3];
    VirtualPointer offsetBaseptr =
        std::bit_cast<VirtualPointer>(reg[baseptrRegIdx]) + constantInnerOffset;
    /// See documentation in "OpCode.h"
    if (offsetCountRegIdx == 0xFF) {
        return offsetBaseptr;
    }
    i64 offsetCount = static_cast<i64>(reg[offsetCountRegIdx]);
    return offsetBaseptr + offsetCount * constantOffsetMultiplier;
}

#define CHECK_ALIGNED(Kind, ptr, Size)                                         \
    do {                                                                       \
        if (SVM_UNLIKELY(!isAligned(ptr, Size))) {                             \
            throwError<MemoryAccessError>(                                     \
                UTL_CONCAT(MemoryAccessError::Misaligned, Kind),               \
                ptr,                                                           \
                Size);                                                         \
        }                                                                      \
    } while (0)

template <size_t Size>
static void moveMR(VirtualMemory& memory, u8 const* i, u64* reg) {
    VirtualPointer ptr = getPointer(reg, i);
    CHECK_ALIGNED(Load, ptr, Size);
    size_t const sourceRegIdx = i[4];
    std::memcpy(memory.dereference(ptr, Size), &reg[sourceRegIdx], Size);
}

template <size_t Size>
static void moveRM(VirtualMemory& memory, u8 const* i, u64* reg) {
    size_t const destRegIdx = i[0];
    VirtualPointer ptr = getPointer(reg, i + 1);
    CHECK_ALIGNED(Store, ptr, Size);
    reg[destRegIdx] = 0;
    std::memcpy(&reg[destRegIdx], memory.dereference(ptr, Size), Size);
}

static void condMove64RR(u8 const* i, u64* reg, bool cond) {
    size_t const destRegIdx = i[0];
    size_t const sourceRegIdx = i[1];
    if (cond) {
        reg[destRegIdx] = reg[sourceRegIdx];
    }
}

static void condMove64RV(u8 const* i, u64* reg, bool cond) {
    size_t const destRegIdx = i[0];
    if (cond) {
        reg[destRegIdx] = load<u64>(i + 1);
    }
}

template <size_t Size>
static void condMoveRM(VirtualMemory& memory,
                       u8 const* i,
                       u64* reg,
                       bool cond) {
    size_t const destRegIdx = i[0];
    VirtualPointer ptr = getPointer(reg, i + 1);
    if (cond) {
        CHECK_ALIGNED(Load, ptr, Size);
        reg[destRegIdx] = 0;
        std::memcpy(&reg[destRegIdx], memory.dereference(ptr, Size), Size);
    }
}

template <OpCode C>
static void performCall(VirtualMemory& memory,
                        u8 const* i,
                        u8 const* binary,
                        ExecutionFrame& currentFrame) {
    auto const [dest, regOffset] = [&] {
        if constexpr (C == OpCode::call) {
            /// Yes, unlike in the indirect call cases we load a 32 bit dest
            /// address here
            return std::pair{ load<u32>(i), load<u8>(i + 4) };
        }
        else if constexpr (C == OpCode::icallr) {
            auto* reg = currentFrame.regPtr;
            u8 const idx = load<u8>(i);
            return std::pair{ load<u64>(reg + idx), load<u8>(i + 1) };
        }
        else {
            static_assert(C == OpCode::icallm);
            auto* reg = currentFrame.regPtr;
            VirtualPointer destAddr = getPointer(reg, i);
            return std::pair{ load<u64>(memory.dereference(destAddr, 8)),
                              load<u8>(i + 4) };
        }
    }();
    currentFrame.regPtr += regOffset;
    currentFrame.regPtr[-3] = utl::bit_cast<u64>(currentFrame.stackPtr);
    currentFrame.regPtr[-2] = regOffset;
    auto* retAddr = currentFrame.iptr + CodeSize<C>;
    currentFrame.regPtr[-1] = utl::bit_cast<u64>(retAddr);
    currentFrame.iptr = binary + dest;
}

template <OpCode C>
static void jump(u8 const* i,
                 u8 const* binary,
                 ExecutionFrame& currentFrame,
                 bool cond) {
    u32 dest = load<u32>(&i[0]);
    if (cond) {
        /// `ExecCodeSize` is added to the instruction pointer after executing
        /// any instruction. Because we want the instruction pointer to be
        /// `binary + dest` we subtract that number here.
        currentFrame.iptr = binary + dest - ExecCodeSize<C>;
    }
}

template <typename T>
static void compareRR(u8 const* i, u64* reg, CompareFlags& flags) {
    size_t const regIdxA = i[0];
    size_t const regIdxB = i[1];
    T const a = load<T>(&reg[regIdxA]);
    T const b = load<T>(&reg[regIdxB]);
    flags.less = a < b;
    flags.equal = a == b;
}

template <typename T>
static void compareRV(u8 const* i, u64* reg, CompareFlags& flags) {
    size_t const regIdxA = i[0];
    T const a = load<T>(&reg[regIdxA]);
    T const b = load<T>(i + 1);
    flags.less = a < b;
    flags.equal = a == b;
}

template <typename T>
static void testR(u8 const* i, u64* reg, CompareFlags& flags) {
    size_t const regIdx = i[0];
    T const a = load<T>(&reg[regIdx]);
    flags.less = a < 0;
    flags.equal = a == 0;
}

static void set(u8 const* i, u64* reg, bool value) {
    size_t const regIdx = i[0];
    storeReg(&reg[regIdx], value);
}

template <typename T>
static void unaryR(u8 const* i, u64* reg, auto operation) {
    size_t const regIdx = i[0];
    auto const a = load<T>(&reg[regIdx]);
    storeReg(&reg[regIdx], decltype(operation)()(a));
}

template <typename T>
static void arithmeticRR(u8 const* i, u64* reg, auto operation) {
    size_t const regIdxA = i[0];
    size_t const regIdxB = i[1];
    auto const a = load<T>(&reg[regIdxA]);
    auto const b = load<T>(&reg[regIdxB]);
    storeReg(&reg[regIdxA], decltype(operation)()(a, b));
}

template <typename LhsType, typename RhsType = LhsType>
static void arithmeticRV(u8 const* i, u64* reg, auto operation) {
    size_t const regIdx = i[0];
    auto const a = load<LhsType>(&reg[regIdx]);
    auto const b = load<RhsType>(i + 1);
    storeReg(&reg[regIdx], static_cast<LhsType>(decltype(operation)()(a, b)));
}

template <typename T>
static void arithmeticRM(VirtualMemory& memory,
                         u8 const* i,
                         u64* reg,
                         auto operation) {
    size_t const regIdxA = i[0];
    VirtualPointer ptr = getPointer(reg, i + 1);
    CHECK_ALIGNED(Load, ptr, alignof(T));
    auto const a = load<T>(&reg[regIdxA]);
    auto const b = load<T>(memory.dereference(ptr, sizeof(T)));
    storeReg(&reg[regIdxA], decltype(operation)()(a, b));
}

static void sext1(u8 const* i, u64* reg) {
    size_t const regIdx = i[0];
    auto const a = load<int>(&reg[regIdx]);
    storeReg(&reg[regIdx], a & 1 ? static_cast<u64>(-1ull) : 0);
}

template <typename From, typename To>
static void convert(u8 const* i, u64* reg) {
    size_t const regIdx = i[0];
    auto const a = load<From>(&reg[regIdx]);
    storeReg(&reg[regIdx], static_cast<To>(a));
}

/// ## Conditions
static bool equal(CompareFlags f) { return f.equal; }
static bool notEqual(CompareFlags f) { return !f.equal; }
static bool less(CompareFlags f) { return f.less; }
static bool lessEq(CompareFlags f) { return f.less || f.equal; }
static bool greater(CompareFlags f) { return !f.less && !f.equal; }
static bool greaterEq(CompareFlags f) { return !f.less; }

static void invokeFFI(ForeignFunction& F, u64* regPtr, VirtualMemory& memory) {
    u64* argPtr = regPtr;
    for (size_t i = 0; i < F.arguments.size(); ++i) {
        if (F.argumentTypes[i] == &ffi_type_pointer ||
            F.argumentTypes[i] == &ArrayPtrType)
        {
            *argPtr = std::bit_cast<u64>(memory.dereferenceNoSize(
                std::bit_cast<VirtualPointer>(*argPtr)));
        }
        F.arguments[i] = argPtr++;
    }
    ffi_call(&F.callInterface, F.funcPtr, regPtr, F.arguments.data());
}

u64 const* VMImpl::execute(size_t start, std::span<u64 const> arguments) {
    beginExecution(start, arguments);
    while (running()) {
        stepExecution();
    }
    return endExecution();
}

void VMImpl::beginExecution(size_t start, std::span<u64 const> arguments) {
    auto const lastframe = execFrames.top() = currentFrame;
    /// We add `MaxCallframeRegisterCount` to the register pointer because
    /// we have no way of knowing how many registers the currently running
    /// execution frame uses, so we have to assume the worst.
    currentFrame = execFrames.push(ExecutionFrame{
        .regPtr = lastframe.regPtr + VirtualMachine::MaxCallframeRegisterCount,
        .bottomReg =
            lastframe.regPtr + VirtualMachine::MaxCallframeRegisterCount,
        .iptr = binary + start,
        .stackPtr = lastframe.stackPtr });
    std::memcpy(currentFrame.regPtr,
                arguments.data(),
                arguments.size() * sizeof(u64));
}

bool VMImpl::running() const { return currentFrame.iptr < programBreak; }

void VMImpl::stepExecution() {
    OpCode const opcode = load<OpCode>(currentFrame.iptr);
    auto* const i = currentFrame.iptr + sizeof(OpCode);
    auto* const regPtr = currentFrame.regPtr;
    [[maybe_unused]] static constexpr u64 InvalidCodeOffset =
        0xdadadadadadadada;
    size_t codeOffset;
#ifndef NDEBUG
    codeOffset = InvalidCodeOffset;
#endif

    /// The `INST_LIST_BEGIN()` and `INST_LIST_END()` macros exist to avoid
    /// indentation in the switch statement and therefore better formatting.
    /// Kinda hacky but it works nicely.
#define INST_LIST_BEGIN()                                                      \
    switch ((u8)opcode) {                                                      \
        using enum OpCode;

#define INST_LIST_END()                                                        \
    break;                                                                     \
    default:                                                                   \
        throwError<InvalidOpcodeError>((u64)opcode);                           \
        }

    /// Every opcode must be listed with `INST(opcode)` followed by a
    /// statement that is executed for that opcode.
#define INST(opcode)                                                           \
    break;                                                                     \
    case (u8)opcode:                                                           \
        codeOffset = ExecCodeSize<opcode>;

    INST_LIST_BEGIN()

    INST(call) { performCall<call>(memory, i, binary, currentFrame); }
    INST(icallr) { performCall<icallr>(memory, i, binary, currentFrame); }
    INST(icallm) { performCall<icallm>(memory, i, binary, currentFrame); }

    INST(ret) {
        if UTL_UNLIKELY (currentFrame.bottomReg == regPtr) {
            /// Meaning we are the root of the call tree aka. the main/start
            /// function, so we set the instruction pointer to the program
            /// break to terminate execution.
            currentFrame.iptr = programBreak;
        }
        else {
            currentFrame.iptr = utl::bit_cast<u8 const*>(regPtr[-1]);
            currentFrame.regPtr -= regPtr[-2];
            currentFrame.stackPtr = utl::bit_cast<VirtualPointer>(regPtr[-3]);
        }
    }

    INST(cfng) {
        size_t const regPtrOffset = i[0];
        size_t const index = load<u16>(&i[1]);
        auto& function = foreignFunctionTable[index];
        invokeFFI(function, regPtr + regPtrOffset, memory);
    }

    INST(cbltn) {
        size_t const regPtrOffset = i[0];
        size_t const index = load<u16>(&i[1]);
        builtinFunctionTable[index].invoke(regPtr + regPtrOffset, parent);
    }

    INST(terminate) { currentFrame.iptr = programBreak; }

    /// ## Loads and storeRegs
    INST(mov64RR) {
        size_t const destRegIdx = i[0];
        size_t const sourceRegIdx = i[1];
        regPtr[destRegIdx] = regPtr[sourceRegIdx];
    }

    INST(mov64RV) {
        size_t const destRegIdx = i[0];
        regPtr[destRegIdx] = load<u64>(i + 1);
    }

    INST(mov8MR) { moveMR<1>(memory, i, regPtr); }
    INST(mov16MR) { moveMR<2>(memory, i, regPtr); }
    INST(mov32MR) { moveMR<4>(memory, i, regPtr); }
    INST(mov64MR) { moveMR<8>(memory, i, regPtr); }
    INST(mov8RM) { moveRM<1>(memory, i, regPtr); }
    INST(mov16RM) { moveRM<2>(memory, i, regPtr); }
    INST(mov32RM) { moveRM<4>(memory, i, regPtr); }
    INST(mov64RM) { moveRM<8>(memory, i, regPtr); }

    /// ## Conditional moves
    INST(cmove64RR) { condMove64RR(i, regPtr, equal(cmpFlags)); }
    INST(cmove64RV) { condMove64RV(i, regPtr, equal(cmpFlags)); }
    INST(cmove8RM) { condMoveRM<1>(memory, i, regPtr, equal(cmpFlags)); }
    INST(cmove16RM) { condMoveRM<2>(memory, i, regPtr, equal(cmpFlags)); }
    INST(cmove32RM) { condMoveRM<4>(memory, i, regPtr, equal(cmpFlags)); }
    INST(cmove64RM) { condMoveRM<8>(memory, i, regPtr, equal(cmpFlags)); }

    INST(cmovne64RR) { condMove64RR(i, regPtr, notEqual(cmpFlags)); }
    INST(cmovne64RV) { condMove64RV(i, regPtr, notEqual(cmpFlags)); }
    INST(cmovne8RM) { condMoveRM<1>(memory, i, regPtr, notEqual(cmpFlags)); }
    INST(cmovne16RM) { condMoveRM<2>(memory, i, regPtr, notEqual(cmpFlags)); }
    INST(cmovne32RM) { condMoveRM<4>(memory, i, regPtr, notEqual(cmpFlags)); }
    INST(cmovne64RM) { condMoveRM<8>(memory, i, regPtr, notEqual(cmpFlags)); }

    INST(cmovl64RR) { condMove64RR(i, regPtr, less(cmpFlags)); }
    INST(cmovl64RV) { condMove64RV(i, regPtr, less(cmpFlags)); }
    INST(cmovl8RM) { condMoveRM<1>(memory, i, regPtr, less(cmpFlags)); }
    INST(cmovl16RM) { condMoveRM<2>(memory, i, regPtr, less(cmpFlags)); }
    INST(cmovl32RM) { condMoveRM<4>(memory, i, regPtr, less(cmpFlags)); }
    INST(cmovl64RM) { condMoveRM<8>(memory, i, regPtr, less(cmpFlags)); }

    INST(cmovle64RR) { condMove64RR(i, regPtr, lessEq(cmpFlags)); }
    INST(cmovle64RV) { condMove64RV(i, regPtr, lessEq(cmpFlags)); }
    INST(cmovle8RM) { condMoveRM<1>(memory, i, regPtr, lessEq(cmpFlags)); }
    INST(cmovle16RM) { condMoveRM<2>(memory, i, regPtr, lessEq(cmpFlags)); }
    INST(cmovle32RM) { condMoveRM<4>(memory, i, regPtr, lessEq(cmpFlags)); }
    INST(cmovle64RM) { condMoveRM<8>(memory, i, regPtr, lessEq(cmpFlags)); }

    INST(cmovg64RR) { condMove64RR(i, regPtr, greater(cmpFlags)); }
    INST(cmovg64RV) { condMove64RV(i, regPtr, greater(cmpFlags)); }
    INST(cmovg8RM) { condMoveRM<1>(memory, i, regPtr, greater(cmpFlags)); }
    INST(cmovg16RM) { condMoveRM<2>(memory, i, regPtr, greater(cmpFlags)); }
    INST(cmovg32RM) { condMoveRM<4>(memory, i, regPtr, greater(cmpFlags)); }
    INST(cmovg64RM) { condMoveRM<8>(memory, i, regPtr, greater(cmpFlags)); }

    INST(cmovge64RR) { condMove64RR(i, regPtr, greaterEq(cmpFlags)); }
    INST(cmovge64RV) { condMove64RV(i, regPtr, greaterEq(cmpFlags)); }
    INST(cmovge8RM) { condMoveRM<1>(memory, i, regPtr, greaterEq(cmpFlags)); }
    INST(cmovge16RM) { condMoveRM<2>(memory, i, regPtr, greaterEq(cmpFlags)); }
    INST(cmovge32RM) { condMoveRM<4>(memory, i, regPtr, greaterEq(cmpFlags)); }
    INST(cmovge64RM) { condMoveRM<8>(memory, i, regPtr, greaterEq(cmpFlags)); }

    /// ## Stack pointer manipulation
    INST(lincsp) {
        size_t const destRegIdx = load<u8>(i);
        size_t const offset = load<u16>(i + 1);
        if (SVM_UNLIKELY(offset % 8 != 0)) {
            throwError<InvalidStackAllocationError>(offset);
        }
        regPtr[destRegIdx] = utl::bit_cast<u64>(currentFrame.stackPtr);
        currentFrame.stackPtr += offset;
    }

    /// ## Address calculation
    INST(lea) {
        size_t const destRegIdx = load<u8>(i);
        VirtualPointer ptr = getPointer(regPtr, i + 1);
        regPtr[destRegIdx] = utl::bit_cast<u64>(ptr);
    }

    /// ## Jumps
    INST(jmp) { jump<jmp>(i, binary, currentFrame, true); }
    INST(je) { jump<je>(i, binary, currentFrame, equal(cmpFlags)); }
    INST(jne) { jump<jne>(i, binary, currentFrame, notEqual(cmpFlags)); }
    INST(jl) { jump<jl>(i, binary, currentFrame, less(cmpFlags)); }
    INST(jle) { jump<jle>(i, binary, currentFrame, lessEq(cmpFlags)); }
    INST(jg) { jump<jg>(i, binary, currentFrame, greater(cmpFlags)); }
    INST(jge) { jump<jge>(i, binary, currentFrame, greaterEq(cmpFlags)); }

    /// ## Comparison
    INST(ucmp8RR) { compareRR<u8>(i, regPtr, cmpFlags); }
    INST(ucmp16RR) { compareRR<u16>(i, regPtr, cmpFlags); }
    INST(ucmp32RR) { compareRR<u32>(i, regPtr, cmpFlags); }
    INST(ucmp64RR) { compareRR<u64>(i, regPtr, cmpFlags); }

    INST(scmp8RR) { compareRR<i8>(i, regPtr, cmpFlags); }
    INST(scmp16RR) { compareRR<i16>(i, regPtr, cmpFlags); }
    INST(scmp32RR) { compareRR<i32>(i, regPtr, cmpFlags); }
    INST(scmp64RR) { compareRR<i64>(i, regPtr, cmpFlags); }

    INST(ucmp8RV) { compareRV<u8>(i, regPtr, cmpFlags); }
    INST(ucmp16RV) { compareRV<u16>(i, regPtr, cmpFlags); }
    INST(ucmp32RV) { compareRV<u32>(i, regPtr, cmpFlags); }
    INST(ucmp64RV) { compareRV<u64>(i, regPtr, cmpFlags); }

    INST(scmp8RV) { compareRV<i8>(i, regPtr, cmpFlags); }
    INST(scmp16RV) { compareRV<i16>(i, regPtr, cmpFlags); }
    INST(scmp32RV) { compareRV<i32>(i, regPtr, cmpFlags); }
    INST(scmp64RV) { compareRV<i64>(i, regPtr, cmpFlags); }

    INST(fcmp32RR) { compareRR<f32>(i, regPtr, cmpFlags); }
    INST(fcmp64RR) { compareRR<f64>(i, regPtr, cmpFlags); }
    INST(fcmp32RV) { compareRV<f32>(i, regPtr, cmpFlags); }
    INST(fcmp64RV) { compareRV<f64>(i, regPtr, cmpFlags); }

    INST(stest8) { testR<i8>(i, regPtr, cmpFlags); }
    INST(stest16) { testR<i16>(i, regPtr, cmpFlags); }
    INST(stest32) { testR<i32>(i, regPtr, cmpFlags); }
    INST(stest64) { testR<i64>(i, regPtr, cmpFlags); }

    INST(utest8) { testR<u8>(i, regPtr, cmpFlags); }
    INST(utest16) { testR<u16>(i, regPtr, cmpFlags); }
    INST(utest32) { testR<u32>(i, regPtr, cmpFlags); }
    INST(utest64) { testR<u64>(i, regPtr, cmpFlags); }

    /// ## load comparison results
    INST(sete) { set(i, regPtr, equal(cmpFlags)); }
    INST(setne) { set(i, regPtr, notEqual(cmpFlags)); }
    INST(setl) { set(i, regPtr, less(cmpFlags)); }
    INST(setle) { set(i, regPtr, lessEq(cmpFlags)); }
    INST(setg) { set(i, regPtr, greater(cmpFlags)); }
    INST(setge) { set(i, regPtr, greaterEq(cmpFlags)); }

    /// ## Unary operations
    INST(lnt) { unaryR<u64>(i, regPtr, utl::logical_not); }
    INST(bnt) { unaryR<u64>(i, regPtr, utl::bitwise_not); }
    INST(neg8) { unaryR<i8>(i, regPtr, utl::negate); }
    INST(neg16) { unaryR<i16>(i, regPtr, utl::negate); }
    INST(neg32) { unaryR<i32>(i, regPtr, utl::negate); }
    INST(neg64) { unaryR<i64>(i, regPtr, utl::negate); }

    /// ## 64 bit integral arithmetic
    INST(add64RR) { arithmeticRR<u64>(i, regPtr, utl::plus); }
    INST(add64RV) { arithmeticRV<u64>(i, regPtr, utl::plus); }
    INST(add64RM) { arithmeticRM<u64>(memory, i, regPtr, utl::plus); }
    INST(sub64RR) { arithmeticRR<u64>(i, regPtr, utl::minus); }
    INST(sub64RV) { arithmeticRV<u64>(i, regPtr, utl::minus); }
    INST(sub64RM) { arithmeticRM<u64>(memory, i, regPtr, utl::minus); }
    INST(mul64RR) { arithmeticRR<u64>(i, regPtr, utl::multiplies); }
    INST(mul64RV) { arithmeticRV<u64>(i, regPtr, utl::multiplies); }
    INST(mul64RM) { arithmeticRM<u64>(memory, i, regPtr, utl::multiplies); }
    INST(udiv64RR) { arithmeticRR<u64>(i, regPtr, utl::divides); }
    INST(udiv64RV) { arithmeticRV<u64>(i, regPtr, utl::divides); }
    INST(udiv64RM) { arithmeticRM<u64>(memory, i, regPtr, utl::divides); }
    INST(sdiv64RR) { arithmeticRR<i64>(i, regPtr, utl::divides); }
    INST(sdiv64RV) { arithmeticRV<i64>(i, regPtr, utl::divides); }
    INST(sdiv64RM) { arithmeticRM<i64>(memory, i, regPtr, utl::divides); }
    INST(urem64RR) { arithmeticRR<u64>(i, regPtr, utl::modulo); }
    INST(urem64RV) { arithmeticRV<u64>(i, regPtr, utl::modulo); }
    INST(urem64RM) { arithmeticRM<u64>(memory, i, regPtr, utl::modulo); }
    INST(srem64RR) { arithmeticRR<i64>(i, regPtr, utl::modulo); }
    INST(srem64RV) { arithmeticRV<i64>(i, regPtr, utl::modulo); }
    INST(srem64RM) { arithmeticRM<i64>(memory, i, regPtr, utl::modulo); }

    /// ## 32 bit integral arithmetic
    INST(add32RR) { arithmeticRR<u32>(i, regPtr, utl::plus); }
    INST(add32RV) { arithmeticRV<u32>(i, regPtr, utl::plus); }
    INST(add32RM) { arithmeticRM<u32>(memory, i, regPtr, utl::plus); }
    INST(sub32RR) { arithmeticRR<u32>(i, regPtr, utl::minus); }
    INST(sub32RV) { arithmeticRV<u32>(i, regPtr, utl::minus); }
    INST(sub32RM) { arithmeticRM<u32>(memory, i, regPtr, utl::minus); }
    INST(mul32RR) { arithmeticRR<u32>(i, regPtr, utl::multiplies); }
    INST(mul32RV) { arithmeticRV<u32>(i, regPtr, utl::multiplies); }
    INST(mul32RM) { arithmeticRM<u32>(memory, i, regPtr, utl::multiplies); }
    INST(udiv32RR) { arithmeticRR<u32>(i, regPtr, utl::divides); }
    INST(udiv32RV) { arithmeticRV<u32>(i, regPtr, utl::divides); }
    INST(udiv32RM) { arithmeticRM<u32>(memory, i, regPtr, utl::divides); }
    INST(sdiv32RR) { arithmeticRR<i32>(i, regPtr, utl::divides); }
    INST(sdiv32RV) { arithmeticRV<i32>(i, regPtr, utl::divides); }
    INST(sdiv32RM) { arithmeticRM<i32>(memory, i, regPtr, utl::divides); }
    INST(urem32RR) { arithmeticRR<u32>(i, regPtr, utl::modulo); }
    INST(urem32RV) { arithmeticRV<u32>(i, regPtr, utl::modulo); }
    INST(urem32RM) { arithmeticRM<u32>(memory, i, regPtr, utl::modulo); }
    INST(srem32RR) { arithmeticRR<i32>(i, regPtr, utl::modulo); }
    INST(srem32RV) { arithmeticRV<i32>(i, regPtr, utl::modulo); }
    INST(srem32RM) { arithmeticRM<i32>(memory, i, regPtr, utl::modulo); }

    /// ## 64 bit Floating point arithmetic
    INST(fadd64RR) { arithmeticRR<f64>(i, regPtr, utl::plus); }
    INST(fadd64RV) { arithmeticRV<f64>(i, regPtr, utl::plus); }
    INST(fadd64RM) { arithmeticRM<f64>(memory, i, regPtr, utl::plus); }
    INST(fsub64RR) { arithmeticRR<f64>(i, regPtr, utl::minus); }
    INST(fsub64RV) { arithmeticRV<f64>(i, regPtr, utl::minus); }
    INST(fsub64RM) { arithmeticRM<f64>(memory, i, regPtr, utl::minus); }
    INST(fmul64RR) { arithmeticRR<f64>(i, regPtr, utl::multiplies); }
    INST(fmul64RV) { arithmeticRV<f64>(i, regPtr, utl::multiplies); }
    INST(fmul64RM) { arithmeticRM<f64>(memory, i, regPtr, utl::multiplies); }
    INST(fdiv64RR) { arithmeticRR<f64>(i, regPtr, utl::divides); }
    INST(fdiv64RV) { arithmeticRV<f64>(i, regPtr, utl::divides); }
    INST(fdiv64RM) { arithmeticRM<f64>(memory, i, regPtr, utl::divides); }

    /// ## 32 bit Floating point arithmetic
    INST(fadd32RR) { arithmeticRR<f32>(i, regPtr, utl::plus); }
    INST(fadd32RV) { arithmeticRV<f32>(i, regPtr, utl::plus); }
    INST(fadd32RM) { arithmeticRM<f32>(memory, i, regPtr, utl::plus); }
    INST(fsub32RR) { arithmeticRR<f32>(i, regPtr, utl::minus); }
    INST(fsub32RV) { arithmeticRV<f32>(i, regPtr, utl::minus); }
    INST(fsub32RM) { arithmeticRM<f32>(memory, i, regPtr, utl::minus); }
    INST(fmul32RR) { arithmeticRR<f32>(i, regPtr, utl::multiplies); }
    INST(fmul32RV) { arithmeticRV<f32>(i, regPtr, utl::multiplies); }
    INST(fmul32RM) { arithmeticRM<f32>(memory, i, regPtr, utl::multiplies); }
    INST(fdiv32RR) { arithmeticRR<f32>(i, regPtr, utl::divides); }
    INST(fdiv32RV) { arithmeticRV<f32>(i, regPtr, utl::divides); }
    INST(fdiv32RM) { arithmeticRM<f32>(memory, i, regPtr, utl::divides); }

    /// ## 64 bit logical shifts
    INST(lsl64RR) { arithmeticRR<u64>(i, regPtr, utl::leftshift); }
    INST(lsl64RV) { arithmeticRV<u64, u8>(i, regPtr, utl::leftshift); }
    INST(lsl64RM) { arithmeticRM<u64>(memory, i, regPtr, utl::leftshift); }
    INST(lsr64RR) { arithmeticRR<u64>(i, regPtr, utl::rightshift); }
    INST(lsr64RV) { arithmeticRV<u64, u8>(i, regPtr, utl::rightshift); }
    INST(lsr64RM) { arithmeticRM<u64>(memory, i, regPtr, utl::rightshift); }

    /// ## 32 bit logical shifts
    INST(lsl32RR) { arithmeticRR<u32>(i, regPtr, utl::leftshift); }
    INST(lsl32RV) { arithmeticRV<u32, u8>(i, regPtr, utl::leftshift); }
    INST(lsl32RM) { arithmeticRM<u32>(memory, i, regPtr, utl::leftshift); }
    INST(lsr32RR) { arithmeticRR<u32>(i, regPtr, utl::rightshift); }
    INST(lsr32RV) { arithmeticRV<u32, u8>(i, regPtr, utl::rightshift); }
    INST(lsr32RM) { arithmeticRM<u32>(memory, i, regPtr, utl::rightshift); }

    /// ## 64 bit arithmetic shifts
#define ASL utl::arithmetic_leftshift
#define ASR utl::arithmetic_rightshift
    INST(asl64RR) { arithmeticRR<u64>(i, regPtr, ASL); }
    INST(asl64RV) { arithmeticRV<u64, u8>(i, regPtr, ASL); }
    INST(asl64RM) { arithmeticRM<u64>(memory, i, regPtr, ASL); }
    INST(asr64RR) { arithmeticRR<u64>(i, regPtr, ASR); }
    INST(asr64RV) { arithmeticRV<u64, u8>(i, regPtr, ASR); }
    INST(asr64RM) { arithmeticRM<u64>(memory, i, regPtr, ASR); }

    /// ## 32 bit arithmetic shifts
    INST(asl32RR) { arithmeticRR<u32>(i, regPtr, ASL); }
    INST(asl32RV) { arithmeticRV<u32, u8>(i, regPtr, ASL); }
    INST(asl32RM) { arithmeticRM<u32>(memory, i, regPtr, ASL); }
    INST(asr32RR) { arithmeticRR<u32>(i, regPtr, ASR); }
    INST(asr32RV) { arithmeticRV<u32, u8>(i, regPtr, ASR); }
    INST(asr32RM) { arithmeticRM<u32>(memory, i, regPtr, ASR); }

    /// ## 64 bit bitwise operations
    INST(and64RR) { arithmeticRR<u64>(i, regPtr, utl::bitwise_and); }
    INST(and64RV) { arithmeticRV<u64>(i, regPtr, utl::bitwise_and); }
    INST(and64RM) { arithmeticRM<u64>(memory, i, regPtr, utl::bitwise_and); }
    INST(or64RR) { arithmeticRR<u64>(i, regPtr, utl::bitwise_or); }
    INST(or64RV) { arithmeticRV<u64>(i, regPtr, utl::bitwise_or); }
    INST(or64RM) { arithmeticRM<u64>(memory, i, regPtr, utl::bitwise_or); }
    INST(xor64RR) { arithmeticRR<u64>(i, regPtr, utl::bitwise_xor); }
    INST(xor64RV) { arithmeticRV<u64>(i, regPtr, utl::bitwise_xor); }
    INST(xor64RM) { arithmeticRM<u64>(memory, i, regPtr, utl::bitwise_xor); }

    /// ## 32 bit bitwise operations
    INST(and32RR) { arithmeticRR<u32>(i, regPtr, utl::bitwise_and); }
    INST(and32RV) { arithmeticRV<u32>(i, regPtr, utl::bitwise_and); }
    INST(and32RM) { arithmeticRM<u32>(memory, i, regPtr, utl::bitwise_and); }
    INST(or32RR) { arithmeticRR<u32>(i, regPtr, utl::bitwise_or); }
    INST(or32RV) { arithmeticRV<u32>(i, regPtr, utl::bitwise_or); }
    INST(or32RM) { arithmeticRM<u32>(memory, i, regPtr, utl::bitwise_or); }
    INST(xor32RR) { arithmeticRR<u32>(i, regPtr, utl::bitwise_xor); }
    INST(xor32RV) { arithmeticRV<u32>(i, regPtr, utl::bitwise_xor); }
    INST(xor32RM) { arithmeticRM<u32>(memory, i, regPtr, utl::bitwise_xor); }

    /// ## Conversion
    INST(sext1) { ::sext1(i, regPtr); }
    INST(sext8) { convert<i8, i64>(i, regPtr); }
    INST(sext16) { convert<i16, i64>(i, regPtr); }
    INST(sext32) { convert<i32, i64>(i, regPtr); }
    INST(fext) { convert<f32, f64>(i, regPtr); }
    INST(ftrunc) { convert<f64, f32>(i, regPtr); }

    INST(s8tof32) { convert<i8, f32>(i, regPtr); }
    INST(s16tof32) { convert<i16, f32>(i, regPtr); }
    INST(s32tof32) { convert<i32, f32>(i, regPtr); }
    INST(s64tof32) { convert<i64, f32>(i, regPtr); }
    INST(u8tof32) { convert<u8, f32>(i, regPtr); }
    INST(u16tof32) { convert<u16, f32>(i, regPtr); }
    INST(u32tof32) { convert<u32, f32>(i, regPtr); }
    INST(u64tof32) { convert<u64, f32>(i, regPtr); }
    INST(s8tof64) { convert<i8, f64>(i, regPtr); }
    INST(s16tof64) { convert<i16, f64>(i, regPtr); }
    INST(s32tof64) { convert<i32, f64>(i, regPtr); }
    INST(s64tof64) { convert<i64, f64>(i, regPtr); }
    INST(u8tof64) { convert<u8, f64>(i, regPtr); }
    INST(u16tof64) { convert<u16, f64>(i, regPtr); }
    INST(u32tof64) { convert<u32, f64>(i, regPtr); }
    INST(u64tof64) { convert<u64, f64>(i, regPtr); }

    INST(f32tos8) { convert<f32, i8>(i, regPtr); }
    INST(f32tos16) { convert<f32, i16>(i, regPtr); }
    INST(f32tos32) { convert<f32, i32>(i, regPtr); }
    INST(f32tos64) { convert<f32, i64>(i, regPtr); }
    INST(f32tou8) { convert<f32, u8>(i, regPtr); }
    INST(f32tou16) { convert<f32, u16>(i, regPtr); }
    INST(f32tou32) { convert<f32, u32>(i, regPtr); }
    INST(f32tou64) { convert<f32, u64>(i, regPtr); }
    INST(f64tos8) { convert<f64, i8>(i, regPtr); }
    INST(f64tos16) { convert<f64, i16>(i, regPtr); }
    INST(f64tos32) { convert<f64, i32>(i, regPtr); }
    INST(f64tos64) { convert<f64, i64>(i, regPtr); }
    INST(f64tou8) { convert<f64, u8>(i, regPtr); }
    INST(f64tou16) { convert<f64, u16>(i, regPtr); }
    INST(f64tou32) { convert<f64, u32>(i, regPtr); }
    INST(f64tou64) { convert<f64, u64>(i, regPtr); }

    INST_LIST_END()

    /// This is an assert because it means we forgot to set the offset in one of
    /// the opcode cases
    assert(codeOffset != InvalidCodeOffset);
    currentFrame.iptr += codeOffset;
    ++stats.executedInstructions;
}

u64 const* VMImpl::endExecution() {
    execFrames.pop();
    auto* result = currentFrame.regPtr;
    currentFrame = execFrames.top();
    return result;
}

size_t VMImpl::instructionPointerOffset() const {
    return utl::narrow_cast<size_t>(currentFrame.iptr - binary);
}

void VMImpl::setInstructionPointerOffset(size_t offset) {
    currentFrame.iptr = binary + offset;
}
