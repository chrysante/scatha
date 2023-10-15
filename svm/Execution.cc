#include <svm/VirtualMachine.h>

#include <cassert>

#include <utl/functional.hpp>

#include "svm/Memory.h"
#include "svm/OpCodeInternal.h"

#if defined(__GNUC__)

#define ALWAYS_INLINE __attribute__((always_inline))

#define SVM_ASSERT(COND) assert(COND)

#define SVM_UNREACHABLE() __builtin_unreachable()

#elif defined(_MSC_VER)

#define ALWAYS_INLINE

#define SVM_ASSERT(COND) assert(COND)

#define SVM_UNREACHABLE() assert(false)

#else

#error Unsupported compiler

#endif

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
ALWAYS_INLINE static void storeReg(u64* dest, T const& t) {
    std::memset(dest, 0, 8);
    std::memcpy(dest, &t, sizeof(T));
}

ALWAYS_INLINE static u8* getPointer(u64 const* reg, u8 const* i) {
    size_t const baseptrRegIdx = i[0];
    size_t const offsetCountRegIdx = i[1];
    i64 const constantOffsetMultiplier = i[2];
    i64 const constantInnerOffset = i[3];
    u8* const offsetBaseptr =
        utl::bit_cast<u8*>(reg[baseptrRegIdx]) + constantInnerOffset;
    /// See documentation in "OpCode.h"
    if (offsetCountRegIdx == 0xFF) {
        return offsetBaseptr;
    }
    i64 const offsetCount = static_cast<i64>(reg[offsetCountRegIdx]);
    return offsetBaseptr + offsetCount * constantOffsetMultiplier;
}

template <size_t Size>
ALWAYS_INLINE static void moveMR(u8 const* i, u64* reg) {
    u8* const ptr = getPointer(reg, i);
    size_t const sourceRegIdx = i[4];
    SVM_ASSERT(reinterpret_cast<size_t>(ptr) % Size == 0);
    std::memcpy(ptr, &reg[sourceRegIdx], Size);
}

template <size_t Size>
ALWAYS_INLINE static void moveRM(u8 const* i, u64* reg) {
    size_t const destRegIdx = i[0];
    u8* const ptr = getPointer(reg, i + 1);
    SVM_ASSERT(reinterpret_cast<size_t>(ptr) % Size == 0);
    reg[destRegIdx] = 0;
    std::memcpy(&reg[destRegIdx], ptr, Size);
}

ALWAYS_INLINE static void condMove64RR(u8 const* i, u64* reg, bool cond) {
    size_t const destRegIdx = i[0];
    size_t const sourceRegIdx = i[1];
    if (cond) {
        reg[destRegIdx] = reg[sourceRegIdx];
    }
}

ALWAYS_INLINE static void condMove64RV(u8 const* i, u64* reg, bool cond) {
    size_t const destRegIdx = i[0];
    if (cond) {
        reg[destRegIdx] = load<u64>(i + 1);
    }
}

template <size_t Size>
ALWAYS_INLINE static void condMoveRM(u8 const* i, u64* reg, bool cond) {
    size_t const destRegIdx = i[0];
    u8* const ptr = getPointer(reg, i + 1);
    SVM_ASSERT(reinterpret_cast<size_t>(ptr) % Size == 0);
    if (cond) {
        reg[destRegIdx] = 0;
        std::memcpy(&reg[destRegIdx], ptr, Size);
    }
}

template <OpCode C>
ALWAYS_INLINE static void jump(u8 const* i,
                               u8 const* text,
                               ExecutionFrame& currentFrame,
                               bool cond) {
    u32 dest = load<u32>(&i[0]);
    if (cond) {
        /// `ExecCodeSize` is added to the instruction pointer after executing
        /// any instruction. Because we want the instruction pointer to be
        /// `text + dest` we subtract that number here.
        currentFrame.iptr = text + dest - ExecCodeSize<C>;
    }
}

template <typename T>
ALWAYS_INLINE static void compareRR(u8 const* i, u64* reg, VMFlags& flags) {
    size_t const regIdxA = i[0];
    size_t const regIdxB = i[1];
    T const a = load<T>(&reg[regIdxA]);
    T const b = load<T>(&reg[regIdxB]);
    flags.less = a < b;
    flags.equal = a == b;
}

template <typename T>
ALWAYS_INLINE static void compareRV(u8 const* i, u64* reg, VMFlags& flags) {
    size_t const regIdxA = i[0];
    T const a = load<T>(&reg[regIdxA]);
    T const b = load<T>(i + 1);
    flags.less = a < b;
    flags.equal = a == b;
}

template <typename T>
ALWAYS_INLINE static void testR(u8 const* i, u64* reg, VMFlags& flags) {
    size_t const regIdx = i[0];
    T const a = load<T>(&reg[regIdx]);
    flags.less = a < 0;
    flags.equal = a == 0;
}

ALWAYS_INLINE static void set(u8 const* i, u64* reg, bool value) {
    size_t const regIdx = i[0];
    storeReg(&reg[regIdx], value);
}

template <typename T>
ALWAYS_INLINE static void unaryR(u8 const* i, u64* reg, auto operation) {
    size_t const regIdx = i[0];
    auto const a = load<T>(&reg[regIdx]);
    storeReg(&reg[regIdx], decltype(operation)()(a));
}

template <typename T>
ALWAYS_INLINE static void arithmeticRR(u8 const* i, u64* reg, auto operation) {
    size_t const regIdxA = i[0];
    size_t const regIdxB = i[1];
    auto const a = load<T>(&reg[regIdxA]);
    auto const b = load<T>(&reg[regIdxB]);
    storeReg(&reg[regIdxA], decltype(operation)()(a, b));
}

template <typename LhsType, typename RhsType = LhsType>
ALWAYS_INLINE static void arithmeticRV(u8 const* i, u64* reg, auto operation) {
    size_t const regIdx = i[0];
    auto const a = load<LhsType>(&reg[regIdx]);
    auto const b = load<RhsType>(i + 1);
    storeReg(&reg[regIdx], static_cast<LhsType>(decltype(operation)()(a, b)));
}

template <typename T>
ALWAYS_INLINE static void arithmeticRM(u8 const* i, u64* reg, auto operation) {
    size_t const regIdxA = i[0];
    u8* const ptr = getPointer(reg, i + 1);
    SVM_ASSERT(reinterpret_cast<size_t>(ptr) % 8 == 0);
    auto const a = load<T>(&reg[regIdxA]);
    auto const b = load<T>(ptr);
    storeReg(&reg[regIdxA], decltype(operation)()(a, b));
}

ALWAYS_INLINE static void sext1(u8 const* i, u64* reg) {
    size_t const regIdx = i[0];
    auto const a = load<int>(&reg[regIdx]);
    storeReg(&reg[regIdx], a & 1 ? static_cast<u64>(-1ull) : 0);
}

template <typename From, typename To>
ALWAYS_INLINE static void convert(u8 const* i, u64* reg) {
    size_t const regIdx = i[0];
    auto const a = load<From>(&reg[regIdx]);
    storeReg(&reg[regIdx], static_cast<To>(a));
}

/// ## Conditions
ALWAYS_INLINE static bool equal(VMFlags f) { return f.equal; }
ALWAYS_INLINE static bool notEqual(VMFlags f) { return !f.equal; }
ALWAYS_INLINE static bool less(VMFlags f) { return f.less; }
ALWAYS_INLINE static bool lessEq(VMFlags f) { return f.less || f.equal; }
ALWAYS_INLINE static bool greater(VMFlags f) { return !f.less && !f.equal; }
ALWAYS_INLINE static bool greaterEq(VMFlags f) { return !f.less; }

u64 const* VirtualMachine::execute(size_t start,
                                   std::span<u64 const> arguments) {
    auto const lastframe = execFrames.top() = currentFrame;
    /// We add `MaxCallframeRegisterCount` to the register pointer because
    /// we have no way of knowing how many registers the currently running
    /// execution currentFrame uses, so we have to assume the worst.
    currentFrame = execFrames.push(ExecutionFrame{
        .regPtr = lastframe.regPtr + MaxCallframeRegisterCount,
        .bottomReg = lastframe.regPtr + MaxCallframeRegisterCount,
        .iptr = text.data() + start,
        .stackPtr = lastframe.stackPtr });
    std::memcpy(currentFrame.regPtr,
                arguments.data(),
                arguments.size() * sizeof(u64));

    /// The main loop of the execution
    while (currentFrame.iptr < programBreak) {
        OpCode const opcode = load<OpCode>(currentFrame.iptr);
        assert(utl::to_underlying(opcode) <
                   utl::to_underlying(OpCode::_count) &&
               "Invalid op-code");
        auto* const i = currentFrame.iptr + sizeof(OpCode);
        auto* const regPtr = currentFrame.regPtr;
        [[maybe_unused]] static constexpr u64 InvalidCodeOffset =
            0xdadadadadadadada;
        size_t codeOffset;
#ifndef NDEBUG
        codeOffset = InvalidCodeOffset;
#endif

        switch (opcode) {
            using enum OpCode;

#define INSTRUCTION(instcode, ...)                                             \
    case instcode:                                                             \
        __VA_ARGS__;                                                           \
        codeOffset = ExecCodeSize<instcode>;                                   \
        break

            // clang-format off
            
        INSTRUCTION(call, {
            u32 const dest = load<u32>(i);
            size_t const regOffset = i[4];
            currentFrame.regPtr += regOffset;
            currentFrame.regPtr[-3] = utl::bit_cast<u64>(currentFrame.stackPtr);
            currentFrame.regPtr[-2] = regOffset;
            auto* retAddr = currentFrame.iptr + CodeSize<call>;
            currentFrame.regPtr[-1] = utl::bit_cast<u64>(retAddr);
            currentFrame.iptr = text.data() + dest;
        });

        INSTRUCTION(ret, {
            if UTL_UNLIKELY (currentFrame.bottomReg == regPtr) {
                /// Meaning we are the root of the call tree aka. the main/start
                /// function, so we set the instruction pointer to the program
                /// break to terminate execution.
                currentFrame.iptr = programBreak;
            }
            else {
                currentFrame.iptr = utl::bit_cast<u8 const*>(regPtr[-1]);
                currentFrame.regPtr -= regPtr[-2];
                currentFrame.stackPtr = utl::bit_cast<u8*>(regPtr[-3]);
            }
        });

        INSTRUCTION(callExt, {
            size_t const regPtrOffset = i[0];
            size_t const tableIdx     = i[1];
            size_t const idxIntoTable = load<u16>(&i[2]);
            auto const etxFunction = extFunctionTable[tableIdx][idxIntoTable];
            etxFunction.invoke(regPtr + regPtrOffset, this);
        });

        INSTRUCTION(terminate, { currentFrame.iptr = programBreak; });

            /// ## Loads and storeRegs
        INSTRUCTION(mov64RR, {
            size_t const destRegIdx   = i[0];
            size_t const sourceRegIdx = i[1];
            regPtr[destRegIdx]        = regPtr[sourceRegIdx];
        });

        INSTRUCTION(mov64RV, {
            size_t const destRegIdx = i[0];
            regPtr[destRegIdx]      = load<u64>(i + 1);
        });
            
        INSTRUCTION(mov8MR, moveMR<1>(i, regPtr));
        INSTRUCTION(mov16MR, moveMR<2>(i, regPtr));
        INSTRUCTION(mov32MR, moveMR<4>(i, regPtr));
        INSTRUCTION(mov64MR, moveMR<8>(i, regPtr));
        INSTRUCTION(mov8RM, moveRM<1>(i, regPtr));
        INSTRUCTION(mov16RM, moveRM<2>(i, regPtr));
        INSTRUCTION(mov32RM, moveRM<4>(i, regPtr));
        INSTRUCTION(mov64RM, moveRM<8>(i, regPtr));

        /// ## Conditional moves
        INSTRUCTION(cmove64RR, condMove64RR(i, regPtr, equal(flags)));
        INSTRUCTION(cmove64RV, condMove64RV(i, regPtr, equal(flags)));
        INSTRUCTION(cmove8RM, condMoveRM<1>(i, regPtr, equal(flags)));
        INSTRUCTION(cmove16RM, condMoveRM<2>(i, regPtr, equal(flags)));
        INSTRUCTION(cmove32RM, condMoveRM<4>(i, regPtr, equal(flags)));
        INSTRUCTION(cmove64RM, condMoveRM<8>(i, regPtr, equal(flags)));

        INSTRUCTION(cmovne64RR, condMove64RR(i, regPtr, notEqual(flags)));
        INSTRUCTION(cmovne64RV, condMove64RV(i, regPtr, notEqual(flags)));
        INSTRUCTION(cmovne8RM, condMoveRM<1>(i, regPtr, notEqual(flags)));
        INSTRUCTION(cmovne16RM, condMoveRM<2>(i, regPtr, notEqual(flags)));
        INSTRUCTION(cmovne32RM, condMoveRM<4>(i, regPtr, notEqual(flags)));
        INSTRUCTION(cmovne64RM, condMoveRM<8>(i, regPtr, notEqual(flags)));

        INSTRUCTION(cmovl64RR, condMove64RR(i, regPtr, less(flags)));
        INSTRUCTION(cmovl64RV, condMove64RV(i, regPtr, less(flags)));
        INSTRUCTION(cmovl8RM, condMoveRM<1>(i, regPtr, less(flags)));
        INSTRUCTION(cmovl16RM, condMoveRM<2>(i, regPtr, less(flags)));
        INSTRUCTION(cmovl32RM, condMoveRM<4>(i, regPtr, less(flags)));
        INSTRUCTION(cmovl64RM, condMoveRM<8>(i, regPtr, less(flags)));

        INSTRUCTION(cmovle64RR, condMove64RR(i, regPtr, lessEq(flags)));
        INSTRUCTION(cmovle64RV, condMove64RV(i, regPtr, lessEq(flags)));
        INSTRUCTION(cmovle8RM, condMoveRM<1>(i, regPtr, lessEq(flags)));
        INSTRUCTION(cmovle16RM, condMoveRM<2>(i, regPtr, lessEq(flags)));
        INSTRUCTION(cmovle32RM, condMoveRM<4>(i, regPtr, lessEq(flags)));
        INSTRUCTION(cmovle64RM, condMoveRM<8>(i, regPtr, lessEq(flags)));

        INSTRUCTION(cmovg64RR, condMove64RR(i, regPtr, greater(flags)));
        INSTRUCTION(cmovg64RV, condMove64RV(i, regPtr, greater(flags)));
        INSTRUCTION(cmovg8RM, condMoveRM<1>(i, regPtr, greater(flags)));
        INSTRUCTION(cmovg16RM, condMoveRM<2>(i, regPtr, greater(flags)));
        INSTRUCTION(cmovg32RM, condMoveRM<4>(i, regPtr, greater(flags)));
        INSTRUCTION(cmovg64RM, condMoveRM<8>(i, regPtr, greater(flags)));

        INSTRUCTION(cmovge64RR, condMove64RR(i, regPtr, greaterEq(flags)));
        INSTRUCTION(cmovge64RV, condMove64RV(i, regPtr, greaterEq(flags)));
        INSTRUCTION(cmovge8RM, condMoveRM<1>(i, regPtr, greaterEq(flags)));
        INSTRUCTION(cmovge16RM, condMoveRM<2>(i, regPtr, greaterEq(flags)));
        INSTRUCTION(cmovge32RM, condMoveRM<4>(i, regPtr, greaterEq(flags)));
        INSTRUCTION(cmovge64RM, condMoveRM<8>(i, regPtr, greaterEq(flags)));

        /// ## Stack pointer manipulation
        INSTRUCTION(lincsp, {
            size_t const destRegIdx = load<u8>(i);
            size_t const offset     = load<u16>(i + 1);
            SVM_ASSERT(offset % 8 == 0);
            regPtr[destRegIdx] = utl::bit_cast<u64>(currentFrame.stackPtr);
            currentFrame.stackPtr += offset;
        });

        /// ## Address calculation
        INSTRUCTION(lea, {
            size_t const destRegIdx = load<u8>(i);
            u8* const ptr           = getPointer(regPtr, i + 1);
            regPtr[destRegIdx]      = utl::bit_cast<u64>(ptr);
        });
            
        INSTRUCTION(lda, {
            size_t const destRegIdx = load<u8>(i);
            size_t const offset     = load<u32>(&i[1]);
            u8* const address       = data.data() + offset;
            regPtr[destRegIdx]      = utl::bit_cast<u64>(address);
        });

        /// ## Jumps
        INSTRUCTION(jmp, jump<jmp>(i, text.data(), currentFrame, true));
        INSTRUCTION(je, jump<je>(i, text.data(), currentFrame, equal(flags)));
        INSTRUCTION(jne, jump<jne>(i, text.data(), currentFrame, notEqual(flags)));
        INSTRUCTION(jl, jump<jl>(i, text.data(), currentFrame, less(flags)));
        INSTRUCTION(jle, jump<jle>(i, text.data(), currentFrame, lessEq(flags)));
        INSTRUCTION(jg, jump<jg>(i, text.data(), currentFrame, greater(flags)));
        INSTRUCTION(jge, jump<jge>(i, text.data(), currentFrame, greaterEq(flags)));

        /// ## Comparison
        INSTRUCTION(ucmp8RR, compareRR<u8>(i, regPtr, flags));
        INSTRUCTION(ucmp16RR, compareRR<u16>(i, regPtr, flags));
        INSTRUCTION(ucmp32RR, compareRR<u32>(i, regPtr, flags));
        INSTRUCTION(ucmp64RR, compareRR<u64>(i, regPtr, flags));

        INSTRUCTION(scmp8RR, compareRR<i8>(i, regPtr, flags));
        INSTRUCTION(scmp16RR, compareRR<i16>(i, regPtr, flags));
        INSTRUCTION(scmp32RR, compareRR<i32>(i, regPtr, flags));
        INSTRUCTION(scmp64RR, compareRR<i64>(i, regPtr, flags));

        INSTRUCTION(ucmp8RV, compareRV<u8>(i, regPtr, flags));
        INSTRUCTION(ucmp16RV, compareRV<u16>(i, regPtr, flags));
        INSTRUCTION(ucmp32RV, compareRV<u32>(i, regPtr, flags));
        INSTRUCTION(ucmp64RV, compareRV<u64>(i, regPtr, flags));

        INSTRUCTION(scmp8RV, compareRV<i8>(i, regPtr, flags));
        INSTRUCTION(scmp16RV, compareRV<i16>(i, regPtr, flags));
        INSTRUCTION(scmp32RV, compareRV<i32>(i, regPtr, flags));
        INSTRUCTION(scmp64RV, compareRV<i64>(i, regPtr, flags));

        INSTRUCTION(fcmp32RR, compareRR<f32>(i, regPtr, flags));
        INSTRUCTION(fcmp64RR, compareRR<f64>(i, regPtr, flags));
        INSTRUCTION(fcmp32RV, compareRV<f32>(i, regPtr, flags));
        INSTRUCTION(fcmp64RV, compareRV<f64>(i, regPtr, flags));

        INSTRUCTION(stest8, testR<i8>(i, regPtr, flags));
        INSTRUCTION(stest16, testR<i16>(i, regPtr, flags));
        INSTRUCTION(stest32, testR<i32>(i, regPtr, flags));
        INSTRUCTION(stest64, testR<i64>(i, regPtr, flags));

        INSTRUCTION(utest8, testR<u8>(i, regPtr, flags));
        INSTRUCTION(utest16, testR<u16>(i, regPtr, flags));
        INSTRUCTION(utest32, testR<u32>(i, regPtr, flags));
        INSTRUCTION(utest64, testR<u64>(i, regPtr, flags));

        /// ## load comparison results
        INSTRUCTION(sete, set(i, regPtr, equal(flags)));
        INSTRUCTION(setne, set(i, regPtr, notEqual(flags)));
        INSTRUCTION(setl, set(i, regPtr, less(flags)));
        INSTRUCTION(setle, set(i, regPtr, lessEq(flags)));
        INSTRUCTION(setg, set(i, regPtr, greater(flags)));
        INSTRUCTION(setge, set(i, regPtr, greaterEq(flags)));

        /// ## Unary operations
        INSTRUCTION(lnt, unaryR<u64>(i, regPtr, utl::logical_not));
        INSTRUCTION(bnt, unaryR<u64>(i, regPtr, utl::bitwise_not));
        INSTRUCTION(neg8, unaryR<i8>(i, regPtr, utl::negate));
        INSTRUCTION(neg16, unaryR<i16>(i, regPtr, utl::negate));
        INSTRUCTION(neg32, unaryR<i32>(i, regPtr, utl::negate));
        INSTRUCTION(neg64, unaryR<i64>(i, regPtr, utl::negate));

        /// ## 64 bit integral arithmetic
        INSTRUCTION(add64RR, arithmeticRR<u64>(i, regPtr, utl::plus));
        INSTRUCTION(add64RV, arithmeticRV<u64>(i, regPtr, utl::plus));
        INSTRUCTION(add64RM, arithmeticRM<u64>(i, regPtr, utl::plus));
        INSTRUCTION(sub64RR, arithmeticRR<u64>(i, regPtr, utl::minus));
        INSTRUCTION(sub64RV, arithmeticRV<u64>(i, regPtr, utl::minus));
        INSTRUCTION(sub64RM, arithmeticRM<u64>(i, regPtr, utl::minus));
        INSTRUCTION(mul64RR, arithmeticRR<u64>(i, regPtr, utl::multiplies));
        INSTRUCTION(mul64RV, arithmeticRV<u64>(i, regPtr, utl::multiplies));
        INSTRUCTION(mul64RM, arithmeticRM<u64>(i, regPtr, utl::multiplies));
        INSTRUCTION(udiv64RR, arithmeticRR<u64>(i, regPtr, utl::divides));
        INSTRUCTION(udiv64RV, arithmeticRV<u64>(i, regPtr, utl::divides));
        INSTRUCTION(udiv64RM, arithmeticRM<u64>(i, regPtr, utl::divides));
        INSTRUCTION(sdiv64RR, arithmeticRR<i64>(i, regPtr, utl::divides));
        INSTRUCTION(sdiv64RV, arithmeticRV<i64>(i, regPtr, utl::divides));
        INSTRUCTION(sdiv64RM, arithmeticRM<i64>(i, regPtr, utl::divides));
        INSTRUCTION(urem64RR, arithmeticRR<u64>(i, regPtr, utl::modulo));
        INSTRUCTION(urem64RV, arithmeticRV<u64>(i, regPtr, utl::modulo));
        INSTRUCTION(urem64RM, arithmeticRM<u64>(i, regPtr, utl::modulo));
        INSTRUCTION(srem64RR, arithmeticRR<i64>(i, regPtr, utl::modulo));
        INSTRUCTION(srem64RV, arithmeticRV<i64>(i, regPtr, utl::modulo));
        INSTRUCTION(srem64RM, arithmeticRM<i64>(i, regPtr, utl::modulo));

        /// ## 32 bit integral arithmetic
        INSTRUCTION(add32RR, arithmeticRR<u32>(i, regPtr, utl::plus));
        INSTRUCTION(add32RV, arithmeticRV<u32>(i, regPtr, utl::plus));
        INSTRUCTION(add32RM, arithmeticRM<u32>(i, regPtr, utl::plus));
        INSTRUCTION(sub32RR, arithmeticRR<u32>(i, regPtr, utl::minus));
        INSTRUCTION(sub32RV, arithmeticRV<u32>(i, regPtr, utl::minus));
        INSTRUCTION(sub32RM, arithmeticRM<u32>(i, regPtr, utl::minus));
        INSTRUCTION(mul32RR, arithmeticRR<u32>(i, regPtr, utl::multiplies));
        INSTRUCTION(mul32RV, arithmeticRV<u32>(i, regPtr, utl::multiplies));
        INSTRUCTION(mul32RM, arithmeticRM<u32>(i, regPtr, utl::multiplies));
        INSTRUCTION(udiv32RR, arithmeticRR<u32>(i, regPtr, utl::divides));
        INSTRUCTION(udiv32RV, arithmeticRV<u32>(i, regPtr, utl::divides));
        INSTRUCTION(udiv32RM, arithmeticRM<u32>(i, regPtr, utl::divides));
        INSTRUCTION(sdiv32RR, arithmeticRR<i32>(i, regPtr, utl::divides));
        INSTRUCTION(sdiv32RV, arithmeticRV<i32>(i, regPtr, utl::divides));
        INSTRUCTION(sdiv32RM, arithmeticRM<i32>(i, regPtr, utl::divides));
        INSTRUCTION(urem32RR, arithmeticRR<u32>(i, regPtr, utl::modulo));
        INSTRUCTION(urem32RV, arithmeticRV<u32>(i, regPtr, utl::modulo));
        INSTRUCTION(urem32RM, arithmeticRM<u32>(i, regPtr, utl::modulo));
        INSTRUCTION(srem32RR, arithmeticRR<i32>(i, regPtr, utl::modulo));
        INSTRUCTION(srem32RV, arithmeticRV<i32>(i, regPtr, utl::modulo));
        INSTRUCTION(srem32RM, arithmeticRM<i32>(i, regPtr, utl::modulo));

        /// ## 64 bit Floating point arithmetic
        INSTRUCTION(fadd64RR, arithmeticRR<f64>(i, regPtr, utl::plus));
        INSTRUCTION(fadd64RV, arithmeticRV<f64>(i, regPtr, utl::plus));
        INSTRUCTION(fadd64RM, arithmeticRM<f64>(i, regPtr, utl::plus));
        INSTRUCTION(fsub64RR, arithmeticRR<f64>(i, regPtr, utl::minus));
        INSTRUCTION(fsub64RV, arithmeticRV<f64>(i, regPtr, utl::minus));
        INSTRUCTION(fsub64RM, arithmeticRM<f64>(i, regPtr, utl::minus));
        INSTRUCTION(fmul64RR, arithmeticRR<f64>(i, regPtr, utl::multiplies));
        INSTRUCTION(fmul64RV, arithmeticRV<f64>(i, regPtr, utl::multiplies));
        INSTRUCTION(fmul64RM, arithmeticRM<f64>(i, regPtr, utl::multiplies));
        INSTRUCTION(fdiv64RR, arithmeticRR<f64>(i, regPtr, utl::divides));
        INSTRUCTION(fdiv64RV, arithmeticRV<f64>(i, regPtr, utl::divides));
        INSTRUCTION(fdiv64RM, arithmeticRM<f64>(i, regPtr, utl::divides));

        /// ## 32 bit Floating point arithmetic
        INSTRUCTION(fadd32RR, arithmeticRR<f32>(i, regPtr, utl::plus));
        INSTRUCTION(fadd32RV, arithmeticRV<f32>(i, regPtr, utl::plus));
        INSTRUCTION(fadd32RM, arithmeticRM<f32>(i, regPtr, utl::plus));
        INSTRUCTION(fsub32RR, arithmeticRR<f32>(i, regPtr, utl::minus));
        INSTRUCTION(fsub32RV, arithmeticRV<f32>(i, regPtr, utl::minus));
        INSTRUCTION(fsub32RM, arithmeticRM<f32>(i, regPtr, utl::minus));
        INSTRUCTION(fmul32RR, arithmeticRR<f32>(i, regPtr, utl::multiplies));
        INSTRUCTION(fmul32RV, arithmeticRV<f32>(i, regPtr, utl::multiplies));
        INSTRUCTION(fmul32RM, arithmeticRM<f32>(i, regPtr, utl::multiplies));
        INSTRUCTION(fdiv32RR, arithmeticRR<f32>(i, regPtr, utl::divides));
        INSTRUCTION(fdiv32RV, arithmeticRV<f32>(i, regPtr, utl::divides));
        INSTRUCTION(fdiv32RM, arithmeticRM<f32>(i, regPtr, utl::divides));

        /// ## 64 bit logical shifts
        INSTRUCTION(lsl64RR, arithmeticRR<u64>(i, regPtr, utl::leftshift));
        INSTRUCTION(lsl64RV, arithmeticRV<u64, u8>(i, regPtr, utl::leftshift));
        INSTRUCTION(lsl64RM, arithmeticRM<u64>(i, regPtr, utl::leftshift));
        INSTRUCTION(lsr64RR, arithmeticRR<u64>(i, regPtr, utl::rightshift));
        INSTRUCTION(lsr64RV, arithmeticRV<u64, u8>(i, regPtr, utl::rightshift));
        INSTRUCTION(lsr64RM, arithmeticRV<u64>(i, regPtr, utl::rightshift));

        /// ## 32 bit logical shifts
        INSTRUCTION(lsl32RR, arithmeticRR<u32>(i, regPtr, utl::leftshift));
        INSTRUCTION(lsl32RV, arithmeticRV<u32, u8>(i, regPtr, utl::leftshift));
        INSTRUCTION(lsl32RM, arithmeticRM<u32>(i, regPtr, utl::leftshift));
        INSTRUCTION(lsr32RR, arithmeticRR<u32>(i, regPtr, utl::rightshift));
        INSTRUCTION(lsr32RV, arithmeticRV<u32, u8>(i, regPtr, utl::rightshift));
        INSTRUCTION(lsr32RM, arithmeticRV<u32>(i, regPtr, utl::rightshift));

        /// ## 64 bit arithmetic shifts
        INSTRUCTION(asl64RR,
             arithmeticRR<u64>(i, regPtr, utl::arithmetic_leftshift));
        INSTRUCTION(asl64RV,
             arithmeticRV<u64, u8>(i, regPtr, utl::arithmetic_leftshift));
        INSTRUCTION(asl64RM,
             arithmeticRM<u64>(i, regPtr, utl::arithmetic_leftshift));
        INSTRUCTION(asr64RR,
             arithmeticRR<u64>(i, regPtr, utl::arithmetic_rightshift));
        INSTRUCTION(asr64RV,
             arithmeticRV<u64, u8>(i, regPtr, utl::arithmetic_rightshift));
        INSTRUCTION(asr64RM,
             arithmeticRV<u64>(i, regPtr, utl::arithmetic_rightshift));

        /// ## 32 bit arithmetic shifts
        INSTRUCTION(asl32RR,
             arithmeticRR<u32>(i, regPtr, utl::arithmetic_leftshift));
        INSTRUCTION(asl32RV,
             arithmeticRV<u32, u8>(i, regPtr, utl::arithmetic_leftshift));
        INSTRUCTION(asl32RM,
             arithmeticRM<u32>(i, regPtr, utl::arithmetic_leftshift));
        INSTRUCTION(asr32RR,
             arithmeticRR<u32>(i, regPtr, utl::arithmetic_rightshift));
        INSTRUCTION(asr32RV,
             arithmeticRV<u32, u8>(i, regPtr, utl::arithmetic_rightshift));
        INSTRUCTION(asr32RM,
             arithmeticRV<u32>(i, regPtr, utl::arithmetic_rightshift));

        /// ## 64 bit bitwise operations
        INSTRUCTION(and64RR, arithmeticRR<u64>(i, regPtr, utl::bitwise_and));
        INSTRUCTION(and64RV, arithmeticRV<u64>(i, regPtr, utl::bitwise_and));
        INSTRUCTION(and64RM, arithmeticRV<u64>(i, regPtr, utl::bitwise_and));
        INSTRUCTION(or64RR, arithmeticRR<u64>(i, regPtr, utl::bitwise_or));
        INSTRUCTION(or64RV, arithmeticRV<u64>(i, regPtr, utl::bitwise_or));
        INSTRUCTION(or64RM, arithmeticRV<u64>(i, regPtr, utl::bitwise_or));
        INSTRUCTION(xor64RR, arithmeticRR<u64>(i, regPtr, utl::bitwise_xor));
        INSTRUCTION(xor64RV, arithmeticRV<u64>(i, regPtr, utl::bitwise_xor));
        INSTRUCTION(xor64RM, arithmeticRV<u64>(i, regPtr, utl::bitwise_xor));

        /// ## 32 bit bitwise operations
        INSTRUCTION(and32RR, arithmeticRR<u32>(i, regPtr, utl::bitwise_and));
        INSTRUCTION(and32RV, arithmeticRV<u32>(i, regPtr, utl::bitwise_and));
        INSTRUCTION(and32RM, arithmeticRV<u32>(i, regPtr, utl::bitwise_and));
        INSTRUCTION(or32RR, arithmeticRR<u32>(i, regPtr, utl::bitwise_or));
        INSTRUCTION(or32RV, arithmeticRV<u32>(i, regPtr, utl::bitwise_or));
        INSTRUCTION(or32RM, arithmeticRV<u32>(i, regPtr, utl::bitwise_or));
        INSTRUCTION(xor32RR, arithmeticRR<u32>(i, regPtr, utl::bitwise_xor));
        INSTRUCTION(xor32RV, arithmeticRV<u32>(i, regPtr, utl::bitwise_xor));
        INSTRUCTION(xor32RM, arithmeticRV<u32>(i, regPtr, utl::bitwise_xor));

        /// ## Conversion
        INSTRUCTION(sext1, ::sext1(i, regPtr));
        INSTRUCTION(sext8,  convert<i8, i64>(i, regPtr));
        INSTRUCTION(sext16, convert<i16, i64>(i, regPtr));
        INSTRUCTION(sext32, convert<i32, i64>(i, regPtr));
        INSTRUCTION(fext,   convert<f32, f64>(i, regPtr));
        INSTRUCTION(ftrunc, convert<f64, f32>(i, regPtr));
            
        INSTRUCTION(s8tof32,  convert<i8, f32>(i, regPtr));
        INSTRUCTION(s16tof32, convert<i16, f32>(i, regPtr));
        INSTRUCTION(s32tof32, convert<i32, f32>(i, regPtr));
        INSTRUCTION(s64tof32, convert<i64, f32>(i, regPtr));
        INSTRUCTION(u8tof32,  convert<u8, f32>(i, regPtr));
        INSTRUCTION(u16tof32, convert<u16, f32>(i, regPtr));
        INSTRUCTION(u32tof32, convert<u32, f32>(i, regPtr));
        INSTRUCTION(u64tof32, convert<u64, f32>(i, regPtr));
        INSTRUCTION(s8tof64,  convert<i8, f64>(i, regPtr));
        INSTRUCTION(s16tof64, convert<i16, f64>(i, regPtr));
        INSTRUCTION(s32tof64, convert<i32, f64>(i, regPtr));
        INSTRUCTION(s64tof64, convert<i64, f64>(i, regPtr));
        INSTRUCTION(u8tof64,  convert<u8, f64>(i, regPtr));
        INSTRUCTION(u16tof64, convert<u16, f64>(i, regPtr));
        INSTRUCTION(u32tof64, convert<u32, f64>(i, regPtr));
        INSTRUCTION(u64tof64, convert<u64, f64>(i, regPtr));
            
        INSTRUCTION(f32tos8,  convert<f32, i8>(i, regPtr));
        INSTRUCTION(f32tos16, convert<f32, i16>(i, regPtr));
        INSTRUCTION(f32tos32, convert<f32, i32>(i, regPtr));
        INSTRUCTION(f32tos64, convert<f32, i64>(i, regPtr));
        INSTRUCTION(f32tou8,  convert<f32, u8>(i, regPtr));
        INSTRUCTION(f32tou16, convert<f32, u16>(i, regPtr));
        INSTRUCTION(f32tou32, convert<f32, u32>(i, regPtr));
        INSTRUCTION(f32tou64, convert<f32, u64>(i, regPtr));
        INSTRUCTION(f64tos8,  convert<f64, i8>(i, regPtr));
        INSTRUCTION(f64tos16, convert<f64, i16>(i, regPtr));
        INSTRUCTION(f64tos32, convert<f64, i32>(i, regPtr));
        INSTRUCTION(f64tos64, convert<f64, i64>(i, regPtr));
        INSTRUCTION(f64tou8,  convert<f64, u8>(i, regPtr));
        INSTRUCTION(f64tou16, convert<f64, u16>(i, regPtr));
        INSTRUCTION(f64tou32, convert<f64, u32>(i, regPtr));
        INSTRUCTION(f64tou64, convert<f64, u64>(i, regPtr));

            // clang-format on

        case _count:
            SVM_UNREACHABLE();
        }
        SVM_ASSERT(codeOffset != InvalidCodeOffset);
        currentFrame.iptr += codeOffset;
        ++stats.executedInstructions;
    }

    assert(currentFrame.iptr == programBreak);
    execFrames.pop();
    auto* result = currentFrame.regPtr;
    currentFrame = lastframe;
    return result;
}
