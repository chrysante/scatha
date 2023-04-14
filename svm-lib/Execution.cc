#include <svm/VirtualMachine.h>

#include <utl/functional.hpp>

#include "Memory.h"
#include <svm/OpCode.h>

#define ALWAYS_INLINE __attribute__((always_inline))

#define SVM_ASSERT(COND) assert(COND)

using namespace svm;

template <typename T>
ALWAYS_INLINE static void storeReg(u64* dest, T const& t) {
    std::memset(dest, 0, 8);
    std::memcpy(dest, &t, sizeof(T));
}

ALWAYS_INLINE static u8* getPointer(u64 const* reg, u8 const* i) {
    size_t const baseptrRegIdx         = i[0];
    size_t const offsetCountRegIdx     = i[1];
    i64 const constantOffsetMultiplier = i[2];
    i64 const constantInnerOffset      = i[3];
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
    u8* const ptr             = getPointer(reg, i);
    size_t const sourceRegIdx = i[4];
    SVM_ASSERT(reinterpret_cast<size_t>(ptr) % Size == 0);
    std::memcpy(ptr, &reg[sourceRegIdx], Size);
}

template <size_t Size>
ALWAYS_INLINE static void moveRM(u8 const* i, u64* reg) {
    size_t const destRegIdx = i[0];
    u8* const ptr           = getPointer(reg, i + 1);
    SVM_ASSERT(reinterpret_cast<size_t>(ptr) % Size == 0);
    reg[destRegIdx] = 0;
    std::memcpy(&reg[destRegIdx], ptr, Size);
}

ALWAYS_INLINE static void condMove64RR(u8 const* i, u64* reg, bool cond) {
    size_t const destRegIdx   = i[0];
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
    u8* const ptr           = getPointer(reg, i + 1);
    SVM_ASSERT(reinterpret_cast<size_t>(ptr) % Size == 0);
    if (cond) {
        reg[destRegIdx] = 0;
        std::memcpy(&reg[destRegIdx], ptr, Size);
    }
}

template <OpCode C>
ALWAYS_INLINE static void jump(u8 const* i, ExecutionFrame& frame, bool cond) {
    i32 const offset = load<i32>(&i[0]);
    if (cond) {
        frame.iptr += offset - static_cast<i64>(codeSize(C));
    }
}

template <typename T>
ALWAYS_INLINE static void compareRR(u8 const* i, u64* reg, VMFlags& flags) {
    size_t const regIdxA = i[0];
    size_t const regIdxB = i[1];
    T const a            = load<T>(&reg[regIdxA]);
    T const b            = load<T>(&reg[regIdxB]);
    flags.less           = a < b;
    flags.equal          = a == b;
}

template <typename T>
ALWAYS_INLINE static void compareRV(u8 const* i, u64* reg, VMFlags& flags) {
    size_t const regIdxA = i[0];
    T const a            = load<T>(&reg[regIdxA]);
    T const b            = load<T>(i + 1);
    flags.less           = a < b;
    flags.equal          = a == b;
}

template <typename T>
ALWAYS_INLINE static void testR(u8 const* i, u64* reg, VMFlags& flags) {
    size_t const regIdx = i[0];
    T const a           = load<T>(&reg[regIdx]);
    flags.less          = a < 0;
    flags.equal         = a == 0;
}

ALWAYS_INLINE static void set(u8 const* i, u64* reg, bool value) {
    size_t const regIdx = i[0];
    storeReg(&reg[regIdx], value);
}

template <typename T>
ALWAYS_INLINE static void unaryR(u8 const* i, u64* reg, auto operation) {
    size_t const regIdx = i[0];
    auto const a        = load<T>(&reg[regIdx]);
    storeReg(&reg[regIdx], decltype(operation)()(a));
}

template <typename T>
ALWAYS_INLINE static void arithmeticRR(u8 const* i, u64* reg, auto operation) {
    size_t const regIdxA = i[0];
    size_t const regIdxB = i[1];
    auto const a         = load<T>(&reg[regIdxA]);
    auto const b         = load<T>(&reg[regIdxB]);
    storeReg(&reg[regIdxA], decltype(operation)()(a, b));
}

template <typename LhsType, typename RhsType = LhsType>
ALWAYS_INLINE static void arithmeticRV(u8 const* i, u64* reg, auto operation) {
    size_t const regIdx = i[0];
    auto const a        = load<LhsType>(&reg[regIdx]);
    auto const b        = load<RhsType>(i + 1);
    storeReg(&reg[regIdx], static_cast<LhsType>(decltype(operation)()(a, b)));
}

template <typename T>
ALWAYS_INLINE static void arithmeticRM(u8 const* i, u64* reg, auto operation) {
    size_t const regIdxA = i[0];
    u8* const ptr        = getPointer(reg, i + 1);
    SVM_ASSERT(reinterpret_cast<size_t>(ptr) % 8 == 0);
    auto const a = load<T>(&reg[regIdxA]);
    auto const b = load<T>(ptr);
    storeReg(&reg[regIdxA], decltype(operation)()(a, b));
}

ALWAYS_INLINE static void sext1(u8 const* i, u64* reg) {
    size_t const regIdx = i[0];
    auto const a        = load<int>(&reg[regIdx]);
    storeReg(&reg[regIdx], a & 1 ? static_cast<u64>(-1ull) : 0);
}

template <typename From, typename To>
ALWAYS_INLINE static void ext(u8 const* i, u64* reg) {
    size_t const regIdx = i[0];
    auto const a        = load<From>(&reg[regIdx]);
    storeReg(&reg[regIdx], static_cast<To>(a));
}

/// ## Conditions
ALWAYS_INLINE static bool equal(VMFlags f) { return f.equal; }
ALWAYS_INLINE static bool notEqual(VMFlags f) { return !f.equal; }
ALWAYS_INLINE static bool less(VMFlags f) { return f.less; }
ALWAYS_INLINE static bool lessEq(VMFlags f) { return f.less || f.equal; }
ALWAYS_INLINE static bool greater(VMFlags f) { return !f.less && !f.equal; }
ALWAYS_INLINE static bool greaterEq(VMFlags f) { return !f.less; }

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

/// Making this a template actually cuts execution time in debug builds in half.
template <OpCode Code>
static constexpr size_t ExecCodeSize = execCodeSizeImpl(Code);

void VirtualMachine::execute(size_t start, std::span<u64 const> arguments) {
    auto const lastframe = execFrames.top() = frame;
    /// We add `MaxCallframeRegisterCount` to the register pointer because we
    /// have no way of knowing how many registers the currently running
    /// execution frame uses, so we have to assume the worst.
    frame = execFrames.push(ExecutionFrame{
        .regPtr    = lastframe.regPtr + MaxCallframeRegisterCount,
        .bottomReg = lastframe.regPtr + MaxCallframeRegisterCount,
        .iptr      = text.data() + start,
        .stackPtr  = lastframe.stackPtr });
    std::memcpy(frame.regPtr, arguments.data(), arguments.size() * sizeof(u64));
    /// The main loop of the execution
    while (frame.iptr < programBreak) {
        OpCode const opcode                    = load<OpCode>(frame.iptr);
        auto* const i                          = frame.iptr + sizeof(OpCode);
        auto* const regPtr                     = frame.regPtr;
        static constexpr u64 InvalidCodeOffset = 0xdadadadadadadada;
        size_t codeOffset                      = InvalidCodeOffset;

#define INST_RETURN(instcode)                                                  \
    codeOffset = ExecCodeSize<instcode>;                                       \
    break

#define CASE(instcode, ...)                                                    \
    case instcode:                                                             \
        __VA_ARGS__;                                                           \
        INST_RETURN(instcode)

        assert(utl::to_underlying(opcode) <
                   utl::to_underlying(OpCode::_count) &&
               "Invalid op-code");

        switch (opcode) {
            using enum OpCode;

        case call: {
            i32 const offset       = load<i32>(i);
            size_t const regOffset = i[4];
            frame.regPtr += regOffset;
            frame.regPtr[-3] = utl::bit_cast<u64>(frame.stackPtr);
            frame.regPtr[-2] = regOffset;
            frame.regPtr[-1] = utl::bit_cast<u64>(frame.iptr + codeSize(call));
            frame.iptr += offset;
            INST_RETURN(call);
        }

        case ret: {
            if (frame.bottomReg == regPtr) {
                /// Meaning we are the root of the call tree aka. the main/start
                /// function, so we set the instruction pointer to the program
                /// break to terminate execution.
                frame.iptr = programBreak;
                INST_RETURN(ret);
            }
            frame.iptr = utl::bit_cast<u8 const*>(regPtr[-1]);
            frame.regPtr -= regPtr[-2];
            frame.stackPtr = utl::bit_cast<u8*>(regPtr[-3]);
            INST_RETURN(ret);
        }

        case callExt: {
            size_t const regPtrOffset = i[0];
            size_t const tableIdx     = i[1];
            size_t const idxIntoTable = load<u16>(&i[2]);
            auto const etxFunction = extFunctionTable[tableIdx][idxIntoTable];
            etxFunction.funcPtr(regPtr + regPtrOffset,
                                this,
                                etxFunction.context);
            INST_RETURN(callExt);
        }

        case terminate: {
            frame.iptr = programBreak;
            INST_RETURN(terminate);
        }

            /// ## Loads and storeRegs
        case mov64RR: {
            size_t const destRegIdx   = i[0];
            size_t const sourceRegIdx = i[1];
            regPtr[destRegIdx]        = regPtr[sourceRegIdx];
            INST_RETURN(mov64RR);
        }

        case mov64RV: {
            size_t const destRegIdx = i[0];
            regPtr[destRegIdx]      = load<u64>(i + 1);
            INST_RETURN(mov64RV);
        }

            CASE(mov8MR, moveMR<1>(i, regPtr));
            CASE(mov16MR, moveMR<2>(i, regPtr));
            CASE(mov32MR, moveMR<4>(i, regPtr));
            CASE(mov64MR, moveMR<8>(i, regPtr));
            CASE(mov8RM, moveRM<1>(i, regPtr));
            CASE(mov16RM, moveRM<2>(i, regPtr));
            CASE(mov32RM, moveRM<4>(i, regPtr));
            CASE(mov64RM, moveRM<8>(i, regPtr));

            /// ## Conditional moves
            CASE(cmove64RR, condMove64RR(i, regPtr, equal(flags)));
            CASE(cmove64RV, condMove64RV(i, regPtr, equal(flags)));
            CASE(cmove8RM, condMoveRM<1>(i, regPtr, equal(flags)));
            CASE(cmove16RM, condMoveRM<2>(i, regPtr, equal(flags)));
            CASE(cmove32RM, condMoveRM<4>(i, regPtr, equal(flags)));
            CASE(cmove64RM, condMoveRM<8>(i, regPtr, equal(flags)));

            CASE(cmovne64RR, condMove64RR(i, regPtr, notEqual(flags)));
            CASE(cmovne64RV, condMove64RV(i, regPtr, notEqual(flags)));
            CASE(cmovne8RM, condMoveRM<1>(i, regPtr, notEqual(flags)));
            CASE(cmovne16RM, condMoveRM<2>(i, regPtr, notEqual(flags)));
            CASE(cmovne32RM, condMoveRM<4>(i, regPtr, notEqual(flags)));
            CASE(cmovne64RM, condMoveRM<8>(i, regPtr, notEqual(flags)));

            CASE(cmovl64RR, condMove64RR(i, regPtr, less(flags)));
            CASE(cmovl64RV, condMove64RV(i, regPtr, less(flags)));
            CASE(cmovl8RM, condMoveRM<1>(i, regPtr, less(flags)));
            CASE(cmovl16RM, condMoveRM<2>(i, regPtr, less(flags)));
            CASE(cmovl32RM, condMoveRM<4>(i, regPtr, less(flags)));
            CASE(cmovl64RM, condMoveRM<8>(i, regPtr, less(flags)));

            CASE(cmovle64RR, condMove64RR(i, regPtr, lessEq(flags)));
            CASE(cmovle64RV, condMove64RV(i, regPtr, lessEq(flags)));
            CASE(cmovle8RM, condMoveRM<1>(i, regPtr, lessEq(flags)));
            CASE(cmovle16RM, condMoveRM<2>(i, regPtr, lessEq(flags)));
            CASE(cmovle32RM, condMoveRM<4>(i, regPtr, lessEq(flags)));
            CASE(cmovle64RM, condMoveRM<8>(i, regPtr, lessEq(flags)));

            CASE(cmovg64RR, condMove64RR(i, regPtr, greater(flags)));
            CASE(cmovg64RV, condMove64RV(i, regPtr, greater(flags)));
            CASE(cmovg8RM, condMoveRM<1>(i, regPtr, greater(flags)));
            CASE(cmovg16RM, condMoveRM<2>(i, regPtr, greater(flags)));
            CASE(cmovg32RM, condMoveRM<4>(i, regPtr, greater(flags)));
            CASE(cmovg64RM, condMoveRM<8>(i, regPtr, greater(flags)));

            CASE(cmovge64RR, condMove64RR(i, regPtr, greaterEq(flags)));
            CASE(cmovge64RV, condMove64RV(i, regPtr, greaterEq(flags)));
            CASE(cmovge8RM, condMoveRM<1>(i, regPtr, greaterEq(flags)));
            CASE(cmovge16RM, condMoveRM<2>(i, regPtr, greaterEq(flags)));
            CASE(cmovge32RM, condMoveRM<4>(i, regPtr, greaterEq(flags)));
            CASE(cmovge64RM, condMoveRM<8>(i, regPtr, greaterEq(flags)));

            /// ## Stack pointer manipulation
        case lincsp: {
            size_t const destRegIdx = load<u8>(i);
            size_t const offset     = load<u16>(i + 1);
            SVM_ASSERT(offset % 8 == 0);
            regPtr[destRegIdx] = utl::bit_cast<u64>(frame.stackPtr);
            frame.stackPtr += offset;
            INST_RETURN(lincsp);
        }

            /// ## LEA
        case lea: {
            size_t const destRegIdx = load<u8>(i);
            u8* const ptr           = getPointer(regPtr, i + 1);
            regPtr[destRegIdx]      = utl::bit_cast<u64>(ptr);
            INST_RETURN(lea);
        }

            /// ## Jumps
            CASE(jmp, jump<jmp>(i, frame, true));
            CASE(je, jump<je>(i, frame, equal(flags)));
            CASE(jne, jump<jne>(i, frame, notEqual(flags)));
            CASE(jl, jump<jl>(i, frame, less(flags)));
            CASE(jle, jump<jle>(i, frame, lessEq(flags)));
            CASE(jg, jump<jg>(i, frame, greater(flags)));
            CASE(jge, jump<jge>(i, frame, greaterEq(flags)));

            /// ## Comparison
            CASE(ucmp8RR, compareRR<u8>(i, regPtr, flags));
            CASE(ucmp16RR, compareRR<u16>(i, regPtr, flags));
            CASE(ucmp32RR, compareRR<u32>(i, regPtr, flags));
            CASE(ucmp64RR, compareRR<u64>(i, regPtr, flags));

            CASE(scmp8RR, compareRR<i8>(i, regPtr, flags));
            CASE(scmp16RR, compareRR<i16>(i, regPtr, flags));
            CASE(scmp32RR, compareRR<i32>(i, regPtr, flags));
            CASE(scmp64RR, compareRR<i64>(i, regPtr, flags));

            CASE(ucmp8RV, compareRV<u8>(i, regPtr, flags));
            CASE(ucmp16RV, compareRV<u16>(i, regPtr, flags));
            CASE(ucmp32RV, compareRV<u32>(i, regPtr, flags));
            CASE(ucmp64RV, compareRV<u64>(i, regPtr, flags));

            CASE(scmp8RV, compareRV<i8>(i, regPtr, flags));
            CASE(scmp16RV, compareRV<i16>(i, regPtr, flags));
            CASE(scmp32RV, compareRV<i32>(i, regPtr, flags));
            CASE(scmp64RV, compareRV<i64>(i, regPtr, flags));

            CASE(fcmp32RR, compareRR<f32>(i, regPtr, flags));
            CASE(fcmp64RR, compareRR<f64>(i, regPtr, flags));
            CASE(fcmp32RV, compareRV<f32>(i, regPtr, flags));
            CASE(fcmp64RV, compareRV<f64>(i, regPtr, flags));

            CASE(stest8, testR<i8>(i, regPtr, flags));
            CASE(stest16, testR<i16>(i, regPtr, flags));
            CASE(stest32, testR<i32>(i, regPtr, flags));
            CASE(stest64, testR<i64>(i, regPtr, flags));

            CASE(utest8, testR<u8>(i, regPtr, flags));
            CASE(utest16, testR<u16>(i, regPtr, flags));
            CASE(utest32, testR<u32>(i, regPtr, flags));
            CASE(utest64, testR<u64>(i, regPtr, flags));

            /// ## load comparison results
            CASE(sete, set(i, regPtr, equal(flags)));
            CASE(setne, set(i, regPtr, notEqual(flags)));
            CASE(setl, set(i, regPtr, less(flags)));
            CASE(setle, set(i, regPtr, lessEq(flags)));
            CASE(setg, set(i, regPtr, greater(flags)));
            CASE(setge, set(i, regPtr, greaterEq(flags)));

            /// ## Unary operations
            CASE(lnt, unaryR<u64>(i, regPtr, utl::logical_not));
            CASE(bnt, unaryR<u64>(i, regPtr, utl::bitwise_not));

            /// ## 64 bit integral arithmetic
            CASE(add64RR, arithmeticRR<u64>(i, regPtr, utl::plus));
            CASE(add64RV, arithmeticRV<u64>(i, regPtr, utl::plus));
            CASE(add64RM, arithmeticRM<u64>(i, regPtr, utl::plus));
            CASE(sub64RR, arithmeticRR<u64>(i, regPtr, utl::minus));
            CASE(sub64RV, arithmeticRV<u64>(i, regPtr, utl::minus));
            CASE(sub64RM, arithmeticRM<u64>(i, regPtr, utl::minus));
            CASE(mul64RR, arithmeticRR<u64>(i, regPtr, utl::multiplies));
            CASE(mul64RV, arithmeticRV<u64>(i, regPtr, utl::multiplies));
            CASE(mul64RM, arithmeticRM<u64>(i, regPtr, utl::multiplies));
            CASE(udiv64RR, arithmeticRR<u64>(i, regPtr, utl::divides));
            CASE(udiv64RV, arithmeticRV<u64>(i, regPtr, utl::divides));
            CASE(udiv64RM, arithmeticRM<u64>(i, regPtr, utl::divides));
            CASE(sdiv64RR, arithmeticRR<i64>(i, regPtr, utl::divides));
            CASE(sdiv64RV, arithmeticRV<i64>(i, regPtr, utl::divides));
            CASE(sdiv64RM, arithmeticRM<i64>(i, regPtr, utl::divides));
            CASE(urem64RR, arithmeticRR<u64>(i, regPtr, utl::modulo));
            CASE(urem64RV, arithmeticRV<u64>(i, regPtr, utl::modulo));
            CASE(urem64RM, arithmeticRM<u64>(i, regPtr, utl::modulo));
            CASE(srem64RR, arithmeticRR<i64>(i, regPtr, utl::modulo));
            CASE(srem64RV, arithmeticRV<i64>(i, regPtr, utl::modulo));
            CASE(srem64RM, arithmeticRM<i64>(i, regPtr, utl::modulo));

            /// ## 32 bit integral arithmetic
            CASE(add32RR, arithmeticRR<u32>(i, regPtr, utl::plus));
            CASE(add32RV, arithmeticRV<u32>(i, regPtr, utl::plus));
            CASE(add32RM, arithmeticRM<u32>(i, regPtr, utl::plus));
            CASE(sub32RR, arithmeticRR<u32>(i, regPtr, utl::minus));
            CASE(sub32RV, arithmeticRV<u32>(i, regPtr, utl::minus));
            CASE(sub32RM, arithmeticRM<u32>(i, regPtr, utl::minus));
            CASE(mul32RR, arithmeticRR<u32>(i, regPtr, utl::multiplies));
            CASE(mul32RV, arithmeticRV<u32>(i, regPtr, utl::multiplies));
            CASE(mul32RM, arithmeticRM<u32>(i, regPtr, utl::multiplies));
            CASE(udiv32RR, arithmeticRR<u32>(i, regPtr, utl::divides));
            CASE(udiv32RV, arithmeticRV<u32>(i, regPtr, utl::divides));
            CASE(udiv32RM, arithmeticRM<u32>(i, regPtr, utl::divides));
            CASE(sdiv32RR, arithmeticRR<i32>(i, regPtr, utl::divides));
            CASE(sdiv32RV, arithmeticRV<i32>(i, regPtr, utl::divides));
            CASE(sdiv32RM, arithmeticRM<i32>(i, regPtr, utl::divides));
            CASE(urem32RR, arithmeticRR<u32>(i, regPtr, utl::modulo));
            CASE(urem32RV, arithmeticRV<u32>(i, regPtr, utl::modulo));
            CASE(urem32RM, arithmeticRM<u32>(i, regPtr, utl::modulo));
            CASE(srem32RR, arithmeticRR<i32>(i, regPtr, utl::modulo));
            CASE(srem32RV, arithmeticRV<i32>(i, regPtr, utl::modulo));
            CASE(srem32RM, arithmeticRM<i32>(i, regPtr, utl::modulo));

            /// ## 64 bit Floating point arithmetic
            CASE(fadd64RR, arithmeticRR<f64>(i, regPtr, utl::plus));
            CASE(fadd64RV, arithmeticRV<f64>(i, regPtr, utl::plus));
            CASE(fadd64RM, arithmeticRM<f64>(i, regPtr, utl::plus));
            CASE(fsub64RR, arithmeticRR<f64>(i, regPtr, utl::minus));
            CASE(fsub64RV, arithmeticRV<f64>(i, regPtr, utl::minus));
            CASE(fsub64RM, arithmeticRM<f64>(i, regPtr, utl::minus));
            CASE(fmul64RR, arithmeticRR<f64>(i, regPtr, utl::multiplies));
            CASE(fmul64RV, arithmeticRV<f64>(i, regPtr, utl::multiplies));
            CASE(fmul64RM, arithmeticRM<f64>(i, regPtr, utl::multiplies));
            CASE(fdiv64RR, arithmeticRR<f64>(i, regPtr, utl::divides));
            CASE(fdiv64RV, arithmeticRV<f64>(i, regPtr, utl::divides));
            CASE(fdiv64RM, arithmeticRM<f64>(i, regPtr, utl::divides));

            /// ## 32 bit Floating point arithmetic
            CASE(fadd32RR, arithmeticRR<f32>(i, regPtr, utl::plus));
            CASE(fadd32RV, arithmeticRV<f32>(i, regPtr, utl::plus));
            CASE(fadd32RM, arithmeticRM<f32>(i, regPtr, utl::plus));
            CASE(fsub32RR, arithmeticRR<f32>(i, regPtr, utl::minus));
            CASE(fsub32RV, arithmeticRV<f32>(i, regPtr, utl::minus));
            CASE(fsub32RM, arithmeticRM<f32>(i, regPtr, utl::minus));
            CASE(fmul32RR, arithmeticRR<f32>(i, regPtr, utl::multiplies));
            CASE(fmul32RV, arithmeticRV<f32>(i, regPtr, utl::multiplies));
            CASE(fmul32RM, arithmeticRM<f32>(i, regPtr, utl::multiplies));
            CASE(fdiv32RR, arithmeticRR<f32>(i, regPtr, utl::divides));
            CASE(fdiv32RV, arithmeticRV<f32>(i, regPtr, utl::divides));
            CASE(fdiv32RM, arithmeticRM<f32>(i, regPtr, utl::divides));

            /// ## 64 bit logical shifts
            CASE(lsl64RR, arithmeticRR<u64>(i, regPtr, utl::leftshift));
            CASE(lsl64RV, arithmeticRV<u64, u8>(i, regPtr, utl::leftshift));
            CASE(lsl64RM, arithmeticRM<u64>(i, regPtr, utl::leftshift));
            CASE(lsr64RR, arithmeticRR<u64>(i, regPtr, utl::rightshift));
            CASE(lsr64RV, arithmeticRV<u64, u8>(i, regPtr, utl::rightshift));
            CASE(lsr64RM, arithmeticRV<u64>(i, regPtr, utl::rightshift));

            /// ## 32 bit logical shifts
            CASE(lsl32RR, arithmeticRR<u32>(i, regPtr, utl::leftshift));
            CASE(lsl32RV, arithmeticRV<u32, u8>(i, regPtr, utl::leftshift));
            CASE(lsl32RM, arithmeticRM<u32>(i, regPtr, utl::leftshift));
            CASE(lsr32RR, arithmeticRR<u32>(i, regPtr, utl::rightshift));
            CASE(lsr32RV, arithmeticRV<u32, u8>(i, regPtr, utl::rightshift));
            CASE(lsr32RM, arithmeticRV<u32>(i, regPtr, utl::rightshift));

            /// ## 64 bit arithmetic shifts
            CASE(asl64RR,
                 arithmeticRR<u64>(i, regPtr, utl::arithmetic_leftshift));
            CASE(asl64RV,
                 arithmeticRV<u64, u8>(i, regPtr, utl::arithmetic_leftshift));
            CASE(asl64RM,
                 arithmeticRM<u64>(i, regPtr, utl::arithmetic_leftshift));
            CASE(asr64RR,
                 arithmeticRR<u64>(i, regPtr, utl::arithmetic_rightshift));
            CASE(asr64RV,
                 arithmeticRV<u64, u8>(i, regPtr, utl::arithmetic_rightshift));
            CASE(asr64RM,
                 arithmeticRV<u64>(i, regPtr, utl::arithmetic_rightshift));

            /// ## 32 bit arithmetic shifts
            CASE(asl32RR,
                 arithmeticRR<u32>(i, regPtr, utl::arithmetic_leftshift));
            CASE(asl32RV,
                 arithmeticRV<u32, u8>(i, regPtr, utl::arithmetic_leftshift));
            CASE(asl32RM,
                 arithmeticRM<u32>(i, regPtr, utl::arithmetic_leftshift));
            CASE(asr32RR,
                 arithmeticRR<u32>(i, regPtr, utl::arithmetic_rightshift));
            CASE(asr32RV,
                 arithmeticRV<u32, u8>(i, regPtr, utl::arithmetic_rightshift));
            CASE(asr32RM,
                 arithmeticRV<u32>(i, regPtr, utl::arithmetic_rightshift));

            /// ## 64 bit bitwise operations
            CASE(and64RR, arithmeticRR<u64>(i, regPtr, utl::bitwise_and));
            CASE(and64RV, arithmeticRV<u64>(i, regPtr, utl::bitwise_and));
            CASE(and64RM, arithmeticRV<u64>(i, regPtr, utl::bitwise_and));
            CASE(or64RR, arithmeticRR<u64>(i, regPtr, utl::bitwise_or));
            CASE(or64RV, arithmeticRV<u64>(i, regPtr, utl::bitwise_or));
            CASE(or64RM, arithmeticRV<u64>(i, regPtr, utl::bitwise_or));
            CASE(xor64RR, arithmeticRR<u64>(i, regPtr, utl::bitwise_xor));
            CASE(xor64RV, arithmeticRV<u64>(i, regPtr, utl::bitwise_xor));
            CASE(xor64RM, arithmeticRV<u64>(i, regPtr, utl::bitwise_xor));

            /// ## 32 bit bitwise operations
            CASE(and32RR, arithmeticRR<u32>(i, regPtr, utl::bitwise_and));
            CASE(and32RV, arithmeticRV<u32>(i, regPtr, utl::bitwise_and));
            CASE(and32RM, arithmeticRV<u32>(i, regPtr, utl::bitwise_and));
            CASE(or32RR, arithmeticRR<u32>(i, regPtr, utl::bitwise_or));
            CASE(or32RV, arithmeticRV<u32>(i, regPtr, utl::bitwise_or));
            CASE(or32RM, arithmeticRV<u32>(i, regPtr, utl::bitwise_or));
            CASE(xor32RR, arithmeticRR<u32>(i, regPtr, utl::bitwise_xor));
            CASE(xor32RV, arithmeticRV<u32>(i, regPtr, utl::bitwise_xor));
            CASE(xor32RM, arithmeticRV<u32>(i, regPtr, utl::bitwise_xor));

            /// ## Conversion
            CASE(sext1, ::sext1(i, regPtr));
            CASE(sext8, ext<i8, i64>(i, regPtr));
            CASE(sext16, ext<i16, i64>(i, regPtr));
            CASE(sext32, ext<i32, i64>(i, regPtr));
            CASE(fext, ext<f32, f64>(i, regPtr));
            CASE(ftrunc, ext<f64, f32>(i, regPtr));

        case _count:
            __builtin_unreachable();
        }
        SVM_ASSERT(codeOffset != InvalidCodeOffset);
        frame.iptr += codeOffset;
        ++stats.executedInstructions;
    }

    assert(frame.iptr == programBreak);
    execFrames.pop();
    frame = lastframe;
}
