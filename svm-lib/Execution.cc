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

#define INST_RETURN(inst)                                                      \
    codeOffset = ExecCodeSize<inst>;                                           \
    break

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

        case mov8MR:
            moveMR<1>(i, regPtr);
            INST_RETURN(mov8MR);
        case mov16MR:
            moveMR<2>(i, regPtr);
            INST_RETURN(mov16MR);
        case mov32MR:
            moveMR<4>(i, regPtr);
            INST_RETURN(mov32MR);
        case mov64MR:
            moveMR<8>(i, regPtr);
            INST_RETURN(mov64MR);
        case mov8RM:
            moveRM<1>(i, regPtr);
            INST_RETURN(mov8RM);
        case mov16RM:
            moveRM<2>(i, regPtr);
            INST_RETURN(mov16RM);
        case mov32RM:
            moveRM<4>(i, regPtr);
            INST_RETURN(mov32RM);
        case mov64RM:
            moveRM<8>(i, regPtr);
            INST_RETURN(mov64RM);

            /// ## Conditional moves
        case cmove64RR:
            condMove64RR(i, regPtr, equal(flags));
            INST_RETURN(cmove64RR);
        case cmove64RV:
            condMove64RV(i, regPtr, equal(flags));
            INST_RETURN(cmove64RV);
        case cmove8RM:
            condMoveRM<1>(i, regPtr, equal(flags));
            INST_RETURN(cmove8RM);
        case cmove16RM:
            condMoveRM<2>(i, regPtr, equal(flags));
            INST_RETURN(cmove16RM);
        case cmove32RM:
            condMoveRM<4>(i, regPtr, equal(flags));
            INST_RETURN(cmove32RM);
        case cmove64RM:
            condMoveRM<8>(i, regPtr, equal(flags));
            INST_RETURN(cmove64RM);

        case cmovne64RR:
            condMove64RR(i, regPtr, notEqual(flags));
            INST_RETURN(cmovne64RR);
        case cmovne64RV:
            condMove64RV(i, regPtr, notEqual(flags));
            INST_RETURN(cmovne64RV);
        case cmovne8RM:
            condMoveRM<1>(i, regPtr, notEqual(flags));
            INST_RETURN(cmovne8RM);
        case cmovne16RM:
            condMoveRM<2>(i, regPtr, notEqual(flags));
            INST_RETURN(cmovne16RM);
        case cmovne32RM:
            condMoveRM<4>(i, regPtr, notEqual(flags));
            INST_RETURN(cmovne32RM);
        case cmovne64RM:
            condMoveRM<8>(i, regPtr, notEqual(flags));
            INST_RETURN(cmovne64RM);

        case cmovl64RR:
            condMove64RR(i, regPtr, less(flags));
            INST_RETURN(cmovl64RR);
        case cmovl64RV:
            condMove64RV(i, regPtr, less(flags));
            INST_RETURN(cmovl64RV);
        case cmovl8RM:
            condMoveRM<1>(i, regPtr, less(flags));
            INST_RETURN(cmovl8RM);
        case cmovl16RM:
            condMoveRM<2>(i, regPtr, less(flags));
            INST_RETURN(cmovl16RM);
        case cmovl32RM:
            condMoveRM<4>(i, regPtr, less(flags));
            INST_RETURN(cmovl32RM);
        case cmovl64RM:
            condMoveRM<8>(i, regPtr, less(flags));
            INST_RETURN(cmovl64RM);

        case cmovle64RR:
            condMove64RR(i, regPtr, lessEq(flags));
            INST_RETURN(cmovle64RR);
        case cmovle64RV:
            condMove64RV(i, regPtr, lessEq(flags));
            INST_RETURN(cmovle64RV);
        case cmovle8RM:
            condMoveRM<1>(i, regPtr, lessEq(flags));
            INST_RETURN(cmovle8RM);
        case cmovle16RM:
            condMoveRM<2>(i, regPtr, lessEq(flags));
            INST_RETURN(cmovle16RM);
        case cmovle32RM:
            condMoveRM<4>(i, regPtr, lessEq(flags));
            INST_RETURN(cmovle32RM);
        case cmovle64RM:
            condMoveRM<8>(i, regPtr, lessEq(flags));
            INST_RETURN(cmovle64RM);

        case cmovg64RR:
            condMove64RR(i, regPtr, greater(flags));
            INST_RETURN(cmovg64RR);
        case cmovg64RV:
            condMove64RV(i, regPtr, greater(flags));
            INST_RETURN(cmovg64RV);
        case cmovg8RM:
            condMoveRM<1>(i, regPtr, greater(flags));
            INST_RETURN(cmovg8RM);
        case cmovg16RM:
            condMoveRM<2>(i, regPtr, greater(flags));
            INST_RETURN(cmovg16RM);
        case cmovg32RM:
            condMoveRM<4>(i, regPtr, greater(flags));
            INST_RETURN(cmovg32RM);
        case cmovg64RM:
            condMoveRM<8>(i, regPtr, greater(flags));
            INST_RETURN(cmovg64RM);

        case cmovge64RR:
            condMove64RR(i, regPtr, greaterEq(flags));
            INST_RETURN(cmovge64RR);
        case cmovge64RV:
            condMove64RV(i, regPtr, greaterEq(flags));
            INST_RETURN(cmovge64RV);
        case cmovge8RM:
            condMoveRM<1>(i, regPtr, greaterEq(flags));
            INST_RETURN(cmovge8RM);
        case cmovge16RM:
            condMoveRM<2>(i, regPtr, greaterEq(flags));
            INST_RETURN(cmovge16RM);
        case cmovge32RM:
            condMoveRM<4>(i, regPtr, greaterEq(flags));
            INST_RETURN(cmovge32RM);
        case cmovge64RM:
            condMoveRM<8>(i, regPtr, greaterEq(flags));
            INST_RETURN(cmovge64RM);

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
        case jmp:
            jump<jmp>(i, frame, true);
            INST_RETURN(jmp);
        case je:
            jump<je>(i, frame, equal(flags));
            INST_RETURN(je);
        case jne:
            jump<jne>(i, frame, notEqual(flags));
            INST_RETURN(jne);
        case jl:
            jump<jl>(i, frame, less(flags));
            INST_RETURN(jl);
        case jle:
            jump<jle>(i, frame, lessEq(flags));
            INST_RETURN(jle);
        case jg:
            jump<jg>(i, frame, greater(flags));
            INST_RETURN(jg);
        case jge:
            jump<jge>(i, frame, greaterEq(flags));
            INST_RETURN(jge);

            /// ## Comparison
        case ucmp8RR:
            compareRR<u8>(i, regPtr, flags);
            INST_RETURN(ucmp8RR);
        case ucmp16RR:
            compareRR<u16>(i, regPtr, flags);
            INST_RETURN(ucmp16RR);
        case ucmp32RR:
            compareRR<u32>(i, regPtr, flags);
            INST_RETURN(ucmp32RR);
        case ucmp64RR:
            compareRR<u64>(i, regPtr, flags);
            INST_RETURN(ucmp64RR);

        case scmp8RR:
            compareRR<i8>(i, regPtr, flags);
            INST_RETURN(scmp8RR);
        case scmp16RR:
            compareRR<i16>(i, regPtr, flags);
            INST_RETURN(scmp16RR);
        case scmp32RR:
            compareRR<i32>(i, regPtr, flags);
            INST_RETURN(scmp32RR);
        case scmp64RR:
            compareRR<i64>(i, regPtr, flags);
            INST_RETURN(scmp64RR);

        case ucmp8RV:
            compareRV<u8>(i, regPtr, flags);
            INST_RETURN(ucmp8RV);
        case ucmp16RV:
            compareRV<u16>(i, regPtr, flags);
            INST_RETURN(ucmp16RV);
        case ucmp32RV:
            compareRV<u32>(i, regPtr, flags);
            INST_RETURN(ucmp32RV);
        case ucmp64RV:
            compareRV<u64>(i, regPtr, flags);
            INST_RETURN(ucmp64RV);

        case scmp8RV:
            compareRV<i8>(i, regPtr, flags);
            INST_RETURN(scmp8RV);
        case scmp16RV:
            compareRV<i16>(i, regPtr, flags);
            INST_RETURN(scmp16RV);
        case scmp32RV:
            compareRV<i32>(i, regPtr, flags);
            INST_RETURN(scmp32RV);
        case scmp64RV:
            compareRV<i64>(i, regPtr, flags);
            INST_RETURN(scmp64RV);

        case fcmp32RR:
            compareRR<f32>(i, regPtr, flags);
            INST_RETURN(fcmp32RR);
        case fcmp64RR:
            compareRR<f64>(i, regPtr, flags);
            INST_RETURN(fcmp64RR);
        case fcmp32RV:
            compareRV<f32>(i, regPtr, flags);
            INST_RETURN(fcmp32RV);
        case fcmp64RV:
            compareRV<f64>(i, regPtr, flags);
            INST_RETURN(fcmp64RV);

        case stest8:
            testR<i8>(i, regPtr, flags);
            INST_RETURN(stest8);
        case stest16:
            testR<i16>(i, regPtr, flags);
            INST_RETURN(stest16);
        case stest32:
            testR<i32>(i, regPtr, flags);
            INST_RETURN(stest32);
        case stest64:
            testR<i64>(i, regPtr, flags);
            INST_RETURN(stest64);

        case utest8:
            testR<u8>(i, regPtr, flags);
            INST_RETURN(utest8);
        case utest16:
            testR<u16>(i, regPtr, flags);
            INST_RETURN(utest16);
        case utest32:
            testR<u32>(i, regPtr, flags);
            INST_RETURN(utest32);
        case utest64:
            testR<u64>(i, regPtr, flags);
            INST_RETURN(utest64);

            /// ## load comparison results
        case sete:
            set(i, regPtr, equal(flags));
            INST_RETURN(sete);
        case setne:
            set(i, regPtr, notEqual(flags));
            INST_RETURN(setne);
        case setl:
            set(i, regPtr, less(flags));
            INST_RETURN(setl);
        case setle:
            set(i, regPtr, lessEq(flags));
            INST_RETURN(setle);
        case setg:
            set(i, regPtr, greater(flags));
            INST_RETURN(setg);
        case setge:
            set(i, regPtr, greaterEq(flags));
            INST_RETURN(setge);

            /// ## Unary operations
        case lnt:
            unaryR<u64>(i, regPtr, utl::logical_not);
            INST_RETURN(lnt);
        case bnt:
            unaryR<u64>(i, regPtr, utl::bitwise_not);
            INST_RETURN(bnt);

            /// ## 64 bit integer arithmetic
        case add64RR:
            arithmeticRR<u64>(i, regPtr, utl::plus);
            INST_RETURN(add64RR);
        case add64RV:
            arithmeticRV<u64>(i, regPtr, utl::plus);
            INST_RETURN(add64RV);
        case add64RM:
            arithmeticRM<u64>(i, regPtr, utl::plus);
            INST_RETURN(add64RM);
        case sub64RR:
            arithmeticRR<u64>(i, regPtr, utl::minus);
            INST_RETURN(sub64RR);
        case sub64RV:
            arithmeticRV<u64>(i, regPtr, utl::minus);
            INST_RETURN(sub64RV);
        case sub64RM:
            arithmeticRM<u64>(i, regPtr, utl::minus);
            INST_RETURN(sub64RM);
        case mul64RR:
            arithmeticRR<u64>(i, regPtr, utl::multiplies);
            INST_RETURN(mul64RR);
        case mul64RV:
            arithmeticRV<u64>(i, regPtr, utl::multiplies);
            INST_RETURN(mul64RV);
        case mul64RM:
            arithmeticRM<u64>(i, regPtr, utl::multiplies);
            INST_RETURN(mul64RM);
        case udiv64RR:
            arithmeticRR<u64>(i, regPtr, utl::divides);
            INST_RETURN(udiv64RR);
        case udiv64RV:
            arithmeticRV<u64>(i, regPtr, utl::divides);
            INST_RETURN(udiv64RV);
        case udiv64RM:
            arithmeticRM<u64>(i, regPtr, utl::divides);
            INST_RETURN(udiv64RM);
        case sdiv64RR:
            arithmeticRR<i64>(i, regPtr, utl::divides);
            INST_RETURN(sdiv64RR);
        case sdiv64RV:
            arithmeticRV<i64>(i, regPtr, utl::divides);
            INST_RETURN(sdiv64RV);
        case sdiv64RM:
            arithmeticRM<i64>(i, regPtr, utl::divides);
            INST_RETURN(sdiv64RM);
        case urem64RR:
            arithmeticRR<u64>(i, regPtr, utl::modulo);
            INST_RETURN(urem64RR);
        case urem64RV:
            arithmeticRV<u64>(i, regPtr, utl::modulo);
            INST_RETURN(urem64RV);
        case urem64RM:
            arithmeticRM<u64>(i, regPtr, utl::modulo);
            INST_RETURN(urem64RM);
        case srem64RR:
            arithmeticRR<i64>(i, regPtr, utl::modulo);
            INST_RETURN(srem64RR);
        case srem64RV:
            arithmeticRV<i64>(i, regPtr, utl::modulo);
            INST_RETURN(srem64RV);
        case srem64RM:
            arithmeticRM<i64>(i, regPtr, utl::modulo);
            INST_RETURN(srem64RM);

            /// ## 32 bit integer arithmetic
        case add32RR:
            arithmeticRR<u32>(i, regPtr, utl::plus);
            INST_RETURN(add32RR);
        case add32RV:
            arithmeticRV<u32>(i, regPtr, utl::plus);
            INST_RETURN(add32RV);
        case add32RM:
            arithmeticRM<u32>(i, regPtr, utl::plus);
            INST_RETURN(add32RM);
        case sub32RR:
            arithmeticRR<u32>(i, regPtr, utl::minus);
            INST_RETURN(sub32RR);
        case sub32RV:
            arithmeticRV<u32>(i, regPtr, utl::minus);
            INST_RETURN(sub32RV);
        case sub32RM:
            arithmeticRM<u32>(i, regPtr, utl::minus);
            INST_RETURN(sub32RM);
        case mul32RR:
            arithmeticRR<u32>(i, regPtr, utl::multiplies);
            INST_RETURN(mul32RR);
        case mul32RV:
            arithmeticRV<u32>(i, regPtr, utl::multiplies);
            INST_RETURN(mul32RV);
        case mul32RM:
            arithmeticRM<u32>(i, regPtr, utl::multiplies);
            INST_RETURN(mul32RM);
        case udiv32RR:
            arithmeticRR<u32>(i, regPtr, utl::divides);
            INST_RETURN(udiv32RR);
        case udiv32RV:
            arithmeticRV<u32>(i, regPtr, utl::divides);
            INST_RETURN(udiv32RV);
        case udiv32RM:
            arithmeticRM<u32>(i, regPtr, utl::divides);
            INST_RETURN(udiv32RM);
        case sdiv32RR:
            arithmeticRR<i32>(i, regPtr, utl::divides);
            INST_RETURN(sdiv32RR);
        case sdiv32RV:
            arithmeticRV<i32>(i, regPtr, utl::divides);
            INST_RETURN(sdiv32RV);
        case sdiv32RM:
            arithmeticRM<i32>(i, regPtr, utl::divides);
            INST_RETURN(sdiv32RM);
        case urem32RR:
            arithmeticRR<u32>(i, regPtr, utl::modulo);
            INST_RETURN(urem32RR);
        case urem32RV:
            arithmeticRV<u32>(i, regPtr, utl::modulo);
            INST_RETURN(urem32RV);
        case urem32RM:
            arithmeticRM<u32>(i, regPtr, utl::modulo);
            INST_RETURN(urem32RM);
        case srem32RR:
            arithmeticRR<i32>(i, regPtr, utl::modulo);
            INST_RETURN(srem32RR);
        case srem32RV:
            arithmeticRV<i32>(i, regPtr, utl::modulo);
            INST_RETURN(srem32RV);
        case srem32RM:
            arithmeticRM<i32>(i, regPtr, utl::modulo);
            INST_RETURN(srem32RM);

            /// ## 64 bit Floating point arithmetic
        case fadd64RR:
            arithmeticRR<f64>(i, regPtr, utl::plus);
            INST_RETURN(fadd64RR);
        case fadd64RV:
            arithmeticRV<f64>(i, regPtr, utl::plus);
            INST_RETURN(fadd64RV);
        case fadd64RM:
            arithmeticRM<f64>(i, regPtr, utl::plus);
            INST_RETURN(fadd64RM);
        case fsub64RR:
            arithmeticRR<f64>(i, regPtr, utl::minus);
            INST_RETURN(fsub64RR);
        case fsub64RV:
            arithmeticRV<f64>(i, regPtr, utl::minus);
            INST_RETURN(fsub64RV);
        case fsub64RM:
            arithmeticRM<f64>(i, regPtr, utl::minus);
            INST_RETURN(fsub64RM);
        case fmul64RR:
            arithmeticRR<f64>(i, regPtr, utl::multiplies);
            INST_RETURN(fmul64RR);
        case fmul64RV:
            arithmeticRV<f64>(i, regPtr, utl::multiplies);
            INST_RETURN(fmul64RV);
        case fmul64RM:
            arithmeticRM<f64>(i, regPtr, utl::multiplies);
            INST_RETURN(fmul64RM);
        case fdiv64RR:
            arithmeticRR<f64>(i, regPtr, utl::divides);
            INST_RETURN(fdiv64RR);
        case fdiv64RV:
            arithmeticRV<f64>(i, regPtr, utl::divides);
            INST_RETURN(fdiv64RV);
        case fdiv64RM:
            arithmeticRM<f64>(i, regPtr, utl::divides);
            INST_RETURN(fdiv64RM);

            /// ## 32 bit Floating point arithmetic
        case fadd32RR:
            arithmeticRR<f32>(i, regPtr, utl::plus);
            INST_RETURN(fadd32RR);
        case fadd32RV:
            arithmeticRV<f32>(i, regPtr, utl::plus);
            INST_RETURN(fadd32RV);
        case fadd32RM:
            arithmeticRM<f32>(i, regPtr, utl::plus);
            INST_RETURN(fadd32RM);
        case fsub32RR:
            arithmeticRR<f32>(i, regPtr, utl::minus);
            INST_RETURN(fsub32RR);
        case fsub32RV:
            arithmeticRV<f32>(i, regPtr, utl::minus);
            INST_RETURN(fsub32RV);
        case fsub32RM:
            arithmeticRM<f32>(i, regPtr, utl::minus);
            INST_RETURN(fsub32RM);
        case fmul32RR:
            arithmeticRR<f32>(i, regPtr, utl::multiplies);
            INST_RETURN(fmul32RR);
        case fmul32RV:
            arithmeticRV<f32>(i, regPtr, utl::multiplies);
            INST_RETURN(fmul32RV);
        case fmul32RM:
            arithmeticRM<f32>(i, regPtr, utl::multiplies);
            INST_RETURN(fmul32RM);
        case fdiv32RR:
            arithmeticRR<f32>(i, regPtr, utl::divides);
            INST_RETURN(fdiv32RR);
        case fdiv32RV:
            arithmeticRV<f32>(i, regPtr, utl::divides);
            INST_RETURN(fdiv32RV);
        case fdiv32RM:
            arithmeticRM<f32>(i, regPtr, utl::divides);
            INST_RETURN(fdiv32RM);

            /// ## 64 bit logical shifts
        case lsl64RR:
            arithmeticRR<u64>(i, regPtr, utl::leftshift);
            INST_RETURN(lsl64RR);
        case lsl64RV:
            arithmeticRV<u64, u8>(i, regPtr, utl::leftshift);
            INST_RETURN(lsl64RV);
        case lsl64RM:
            arithmeticRM<u64>(i, regPtr, utl::leftshift);
            INST_RETURN(lsl64RM);
        case lsr64RR:
            arithmeticRR<u64>(i, regPtr, utl::rightshift);
            INST_RETURN(lsr64RR);
        case lsr64RV:
            arithmeticRV<u64, u8>(i, regPtr, utl::rightshift);
            INST_RETURN(lsr64RV);
        case lsr64RM:
            arithmeticRV<u64>(i, regPtr, utl::rightshift);
            INST_RETURN(lsr64RM);

            /// ## 32 bit logical shifts
        case lsl32RR:
            arithmeticRR<u32>(i, regPtr, utl::leftshift);
            INST_RETURN(lsl32RR);
        case lsl32RV:
            arithmeticRV<u32, u8>(i, regPtr, utl::leftshift);
            INST_RETURN(lsl32RV);
        case lsl32RM:
            arithmeticRM<u32>(i, regPtr, utl::leftshift);
            INST_RETURN(lsl32RM);
        case lsr32RR:
            arithmeticRR<u32>(i, regPtr, utl::rightshift);
            INST_RETURN(lsr32RR);
        case lsr32RV:
            arithmeticRV<u32, u8>(i, regPtr, utl::rightshift);
            INST_RETURN(lsr32RV);
        case lsr32RM:
            arithmeticRV<u32>(i, regPtr, utl::rightshift);
            INST_RETURN(lsr32RM);

            /// ## 64 bit arithmetic shifts
        case asl64RR:
            arithmeticRR<u64>(i, regPtr, utl::arithmetic_leftshift);
            INST_RETURN(asl64RR);
        case asl64RV:
            arithmeticRV<u64, u8>(i, regPtr, utl::arithmetic_leftshift);
            INST_RETURN(asl64RV);
        case asl64RM:
            arithmeticRM<u64>(i, regPtr, utl::arithmetic_leftshift);
            INST_RETURN(asl64RM);
        case asr64RR:
            arithmeticRR<u64>(i, regPtr, utl::arithmetic_rightshift);
            INST_RETURN(asr64RR);
        case asr64RV:
            arithmeticRV<u64, u8>(i, regPtr, utl::arithmetic_rightshift);
            INST_RETURN(asr64RV);
        case asr64RM:
            arithmeticRV<u64>(i, regPtr, utl::arithmetic_rightshift);
            INST_RETURN(asr64RM);

            /// ## 32 bit arithmetic shifts
        case asl32RR:
            arithmeticRR<u32>(i, regPtr, utl::arithmetic_leftshift);
            INST_RETURN(asl32RR);
        case asl32RV:
            arithmeticRV<u32, u8>(i, regPtr, utl::arithmetic_leftshift);
            INST_RETURN(asl32RV);
        case asl32RM:
            arithmeticRM<u32>(i, regPtr, utl::arithmetic_leftshift);
            INST_RETURN(asl32RM);
        case asr32RR:
            arithmeticRR<u32>(i, regPtr, utl::arithmetic_rightshift);
            INST_RETURN(asr32RR);
        case asr32RV:
            arithmeticRV<u32, u8>(i, regPtr, utl::arithmetic_rightshift);
            INST_RETURN(asr32RV);
        case asr32RM:
            arithmeticRV<u32>(i, regPtr, utl::arithmetic_rightshift);
            INST_RETURN(asr32RM);

            /// ## 64 bit bitwise operations
        case and64RR:
            arithmeticRR<u64>(i, regPtr, utl::bitwise_and);
            INST_RETURN(and64RR);
        case and64RV:
            arithmeticRV<u64>(i, regPtr, utl::bitwise_and);
            INST_RETURN(and64RV);
        case and64RM:
            arithmeticRV<u64>(i, regPtr, utl::bitwise_and);
            INST_RETURN(and64RM);
        case or64RR:
            arithmeticRR<u64>(i, regPtr, utl::bitwise_or);
            INST_RETURN(or64RR);
        case or64RV:
            arithmeticRV<u64>(i, regPtr, utl::bitwise_or);
            INST_RETURN(or64RV);
        case or64RM:
            arithmeticRV<u64>(i, regPtr, utl::bitwise_or);
            INST_RETURN(or64RM);
        case xor64RR:
            arithmeticRR<u64>(i, regPtr, utl::bitwise_xor);
            INST_RETURN(xor64RR);
        case xor64RV:
            arithmeticRV<u64>(i, regPtr, utl::bitwise_xor);
            INST_RETURN(xor64RV);
        case xor64RM:
            arithmeticRV<u64>(i, regPtr, utl::bitwise_xor);
            INST_RETURN(xor64RM);

            /// ## 32 bit bitwise operations
        case and32RR:
            arithmeticRR<u32>(i, regPtr, utl::bitwise_and);
            INST_RETURN(and32RR);
        case and32RV:
            arithmeticRV<u32>(i, regPtr, utl::bitwise_and);
            INST_RETURN(and32RV);
        case and32RM:
            arithmeticRV<u32>(i, regPtr, utl::bitwise_and);
            INST_RETURN(and32RM);
        case or32RR:
            arithmeticRR<u32>(i, regPtr, utl::bitwise_or);
            INST_RETURN(or32RR);
        case or32RV:
            arithmeticRV<u32>(i, regPtr, utl::bitwise_or);
            INST_RETURN(or32RV);
        case or32RM:
            arithmeticRV<u32>(i, regPtr, utl::bitwise_or);
            INST_RETURN(or32RM);
        case xor32RR:
            arithmeticRR<u32>(i, regPtr, utl::bitwise_xor);
            INST_RETURN(xor32RR);
        case xor32RV:
            arithmeticRV<u32>(i, regPtr, utl::bitwise_xor);
            INST_RETURN(xor32RV);
        case xor32RM:
            arithmeticRV<u32>(i, regPtr, utl::bitwise_xor);
            INST_RETURN(xor32RM);

            /// ## Conversion
        case sext1:
            ::sext1(i, regPtr);
            INST_RETURN(sext1);
        case sext8:
            ext<i8, i64>(i, regPtr);
            INST_RETURN(sext8);
        case sext16:
            ext<i16, i64>(i, regPtr);
            INST_RETURN(sext16);
        case sext32:
            ext<i32, i64>(i, regPtr);
            INST_RETURN(sext32);
        case fext:
            ext<f32, f64>(i, regPtr);
            INST_RETURN(fext);
        case ftrunc:
            ext<f64, f32>(i, regPtr);
            INST_RETURN(ftrunc);

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
