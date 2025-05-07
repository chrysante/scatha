// NO-INCLUDE-GUARD

// No include guard, because this file may be included multiple times

/// This file defines the main interpreter execution routine. The exact
/// definition depends on the macros `INST_BEGIN()`, `INST_END()` and
/// `TERMINATE_EXECUTION()`. These macros are defined in "Execution.cc", once
/// for jump-threaded and once for switch-based execution.

#ifndef INST_BEGIN
#error INST_BEGIN(opcode) macro must be defined
#define INST_BEGIN(...) // Define here to avoid further errors
#endif

#ifndef INST_END
#error INST_END(opcode) macro must be defined
#define INST_END(...) // Define here to avoid further errors
#endif

#ifndef TERMINATE_EXECUTION
#error TERMINATE_EXECUTION() macro must be defined
#define TERMINATE_EXECUTION() // Define here to avoid further errors
#endif

INST_BEGIN(call) {
    performCall<OpCode::call>(memory, opPtr, binary, iptr, regPtr,
                              currentFrame.stackPtr);
}
INST_END(call)
INST_BEGIN(icallr) {
    performCall<OpCode::icallr>(memory, opPtr, binary, iptr, regPtr,
                                currentFrame.stackPtr);
}
INST_END(icallr)
INST_BEGIN(icallm) {
    performCall<OpCode::icallm>(memory, opPtr, binary, iptr, regPtr,
                                currentFrame.stackPtr);
}
INST_END(icallm)

INST_BEGIN(ret) {
    if (currentFrame.bottomReg == regPtr) {
        /// Meaning we are the root of the call tree aka. the main/start
        /// function, so we set the instruction pointer to the program
        /// break to terminate execution.
        TERMINATE_EXECUTION();
    }
    else {
        iptr = std::bit_cast<u8 const*>(regPtr[-1]);
        currentFrame.stackPtr = std::bit_cast<VirtualPointer>(regPtr[-3]);
        regPtr -= regPtr[-2];
    }
}
INST_END(ret)

INST_BEGIN(cfng) {
    size_t regPtrOffset = opPtr[0];
    size_t index = load<u16>(&opPtr[1]);
    auto& function = foreignFunctionTable[index];
    invokeFFI(function, regPtr + regPtrOffset, memory);
}
INST_END(cfng)

INST_BEGIN(cbltn) {
    size_t regPtrOffset = opPtr[0];
    size_t index = load<u16>(&opPtr[1]);
    try {
        builtinFunctionTable[index].invoke(regPtr + regPtrOffset, parent);
    }
    catch (ExitException const&) {
        TERMINATE_EXECUTION();
    }
}
INST_END(cbltn)

INST_BEGIN(terminate) { TERMINATE_EXECUTION(); }
INST_END(terminate)

INST_BEGIN(trap) { throwException<InterruptException>(); }
INST_END(trap)

/// ## Loads and storeRegs
INST_BEGIN(mov64RR) {
    size_t destRegIdx = opPtr[0];
    size_t sourceRegIdx = opPtr[1];
    regPtr[destRegIdx] = regPtr[sourceRegIdx];
}
INST_END(mov64RR)

INST_BEGIN(mov64RV) {
    size_t destRegIdx = opPtr[0];
    regPtr[destRegIdx] = load<u64>(opPtr + 1);
}
INST_END(mov64RV)

INST_BEGIN(mov8MR) { moveMR<1>(memory, opPtr, regPtr); }
INST_END(mov8MR)
INST_BEGIN(mov16MR) { moveMR<2>(memory, opPtr, regPtr); }
INST_END(mov16MR)
INST_BEGIN(mov32MR) { moveMR<4>(memory, opPtr, regPtr); }
INST_END(mov32MR)
INST_BEGIN(mov64MR) { moveMR<8>(memory, opPtr, regPtr); }
INST_END(mov64MR)
INST_BEGIN(mov8RM) { moveRM<1>(memory, opPtr, regPtr); }
INST_END(mov8RM)
INST_BEGIN(mov16RM) { moveRM<2>(memory, opPtr, regPtr); }
INST_END(mov16RM)
INST_BEGIN(mov32RM) { moveRM<4>(memory, opPtr, regPtr); }
INST_END(mov32RM)
INST_BEGIN(mov64RM) { moveRM<8>(memory, opPtr, regPtr); }
INST_END(mov64RM)

/// ## Conditional moves
INST_BEGIN(cmove64RR) { condMove64RR(opPtr, regPtr, equal(cmpFlags)); }
INST_END(cmove64RR)
INST_BEGIN(cmove64RV) { condMove64RV(opPtr, regPtr, equal(cmpFlags)); }
INST_END(cmove64RV)
INST_BEGIN(cmove8RM) { condMoveRM<1>(memory, opPtr, regPtr, equal(cmpFlags)); }
INST_END(cmove8RM)
INST_BEGIN(cmove16RM) { condMoveRM<2>(memory, opPtr, regPtr, equal(cmpFlags)); }
INST_END(cmove16RM)
INST_BEGIN(cmove32RM) { condMoveRM<4>(memory, opPtr, regPtr, equal(cmpFlags)); }
INST_END(cmove32RM)
INST_BEGIN(cmove64RM) { condMoveRM<8>(memory, opPtr, regPtr, equal(cmpFlags)); }
INST_END(cmove64RM)

INST_BEGIN(cmovne64RR) { condMove64RR(opPtr, regPtr, notEqual(cmpFlags)); }
INST_END(cmovne64RR)
INST_BEGIN(cmovne64RV) { condMove64RV(opPtr, regPtr, notEqual(cmpFlags)); }
INST_END(cmovne64RV)
INST_BEGIN(cmovne8RM) {
    condMoveRM<1>(memory, opPtr, regPtr, notEqual(cmpFlags));
}
INST_END(cmovne8RM)
INST_BEGIN(cmovne16RM) {
    condMoveRM<2>(memory, opPtr, regPtr, notEqual(cmpFlags));
}
INST_END(cmovne16RM)
INST_BEGIN(cmovne32RM) {
    condMoveRM<4>(memory, opPtr, regPtr, notEqual(cmpFlags));
}
INST_END(cmovne32RM)
INST_BEGIN(cmovne64RM) {
    condMoveRM<8>(memory, opPtr, regPtr, notEqual(cmpFlags));
}
INST_END(cmovne64RM)

INST_BEGIN(cmovl64RR) { condMove64RR(opPtr, regPtr, less(cmpFlags)); }
INST_END(cmovl64RR)
INST_BEGIN(cmovl64RV) { condMove64RV(opPtr, regPtr, less(cmpFlags)); }
INST_END(cmovl64RV)
INST_BEGIN(cmovl8RM) { condMoveRM<1>(memory, opPtr, regPtr, less(cmpFlags)); }
INST_END(cmovl8RM)
INST_BEGIN(cmovl16RM) { condMoveRM<2>(memory, opPtr, regPtr, less(cmpFlags)); }
INST_END(cmovl16RM)
INST_BEGIN(cmovl32RM) { condMoveRM<4>(memory, opPtr, regPtr, less(cmpFlags)); }
INST_END(cmovl32RM)
INST_BEGIN(cmovl64RM) { condMoveRM<8>(memory, opPtr, regPtr, less(cmpFlags)); }
INST_END(cmovl64RM)

INST_BEGIN(cmovle64RR) { condMove64RR(opPtr, regPtr, lessEq(cmpFlags)); }
INST_END(cmovle64RR)
INST_BEGIN(cmovle64RV) { condMove64RV(opPtr, regPtr, lessEq(cmpFlags)); }
INST_END(cmovle64RV)
INST_BEGIN(cmovle8RM) {
    condMoveRM<1>(memory, opPtr, regPtr, lessEq(cmpFlags));
}
INST_END(cmovle8RM)
INST_BEGIN(cmovle16RM) {
    condMoveRM<2>(memory, opPtr, regPtr, lessEq(cmpFlags));
}
INST_END(cmovle16RM)
INST_BEGIN(cmovle32RM) {
    condMoveRM<4>(memory, opPtr, regPtr, lessEq(cmpFlags));
}
INST_END(cmovle32RM)
INST_BEGIN(cmovle64RM) {
    condMoveRM<8>(memory, opPtr, regPtr, lessEq(cmpFlags));
}
INST_END(cmovle64RM)

INST_BEGIN(cmovg64RR) { condMove64RR(opPtr, regPtr, greater(cmpFlags)); }
INST_END(cmovg64RR)
INST_BEGIN(cmovg64RV) { condMove64RV(opPtr, regPtr, greater(cmpFlags)); }
INST_END(cmovg64RV)
INST_BEGIN(cmovg8RM) {
    condMoveRM<1>(memory, opPtr, regPtr, greater(cmpFlags));
}
INST_END(cmovg8RM)
INST_BEGIN(cmovg16RM) {
    condMoveRM<2>(memory, opPtr, regPtr, greater(cmpFlags));
}
INST_END(cmovg16RM)
INST_BEGIN(cmovg32RM) {
    condMoveRM<4>(memory, opPtr, regPtr, greater(cmpFlags));
}
INST_END(cmovg32RM)
INST_BEGIN(cmovg64RM) {
    condMoveRM<8>(memory, opPtr, regPtr, greater(cmpFlags));
}
INST_END(cmovg64RM)

INST_BEGIN(cmovge64RR) { condMove64RR(opPtr, regPtr, greaterEq(cmpFlags)); }
INST_END(cmovge64RR)
INST_BEGIN(cmovge64RV) { condMove64RV(opPtr, regPtr, greaterEq(cmpFlags)); }
INST_END(cmovge64RV)
INST_BEGIN(cmovge8RM) {
    condMoveRM<1>(memory, opPtr, regPtr, greaterEq(cmpFlags));
}
INST_END(cmovge8RM)
INST_BEGIN(cmovge16RM) {
    condMoveRM<2>(memory, opPtr, regPtr, greaterEq(cmpFlags));
}
INST_END(cmovge16RM)
INST_BEGIN(cmovge32RM) {
    condMoveRM<4>(memory, opPtr, regPtr, greaterEq(cmpFlags));
}
INST_END(cmovge32RM)
INST_BEGIN(cmovge64RM) {
    condMoveRM<8>(memory, opPtr, regPtr, greaterEq(cmpFlags));
}
INST_END(cmovge64RM)

/// ## Stack pointer manipulation
INST_BEGIN(lincsp) {
    size_t destRegIdx = load<u8>(opPtr);
    size_t offset = load<u16>(opPtr + 1);
    if (SVM_UNLIKELY(offset % 8 != 0)) {
        throwException<InvalidStackAllocationError>(offset);
    }
    regPtr[destRegIdx] = std::bit_cast<u64>(currentFrame.stackPtr);
    currentFrame.stackPtr += offset;
}
INST_END(lincsp)

/// ## Address calculation
INST_BEGIN(lea) {
    size_t destRegIdx = load<u8>(opPtr);
    VirtualPointer ptr = getPointer(regPtr, opPtr + 1);
    regPtr[destRegIdx] = std::bit_cast<u64>(ptr);
}
INST_END(lea)

/// ## Jumps
INST_BEGIN(jmp) { jump<OpCode::jmp>(opPtr, binary, iptr, true); }
INST_END(jmp)
INST_BEGIN(je) { jump<OpCode::je>(opPtr, binary, iptr, equal(cmpFlags)); }
INST_END(je)
INST_BEGIN(jne) { jump<OpCode::jne>(opPtr, binary, iptr, notEqual(cmpFlags)); }
INST_END(jne)
INST_BEGIN(jl) { jump<OpCode::jl>(opPtr, binary, iptr, less(cmpFlags)); }
INST_END(jl)
INST_BEGIN(jle) { jump<OpCode::jle>(opPtr, binary, iptr, lessEq(cmpFlags)); }
INST_END(jle)
INST_BEGIN(jg) { jump<OpCode::jg>(opPtr, binary, iptr, greater(cmpFlags)); }
INST_END(jg)
INST_BEGIN(jge) { jump<OpCode::jge>(opPtr, binary, iptr, greaterEq(cmpFlags)); }
INST_END(jge)

/// ## Comparison
INST_BEGIN(ucmp8RR) { compareRR<u8>(opPtr, regPtr, cmpFlags); }
INST_END(ucmp8RR)
INST_BEGIN(ucmp16RR) { compareRR<u16>(opPtr, regPtr, cmpFlags); }
INST_END(ucmp16RR)
INST_BEGIN(ucmp32RR) { compareRR<u32>(opPtr, regPtr, cmpFlags); }
INST_END(ucmp32RR)
INST_BEGIN(ucmp64RR) { compareRR<u64>(opPtr, regPtr, cmpFlags); }
INST_END(ucmp64RR)

INST_BEGIN(scmp8RR) { compareRR<i8>(opPtr, regPtr, cmpFlags); }
INST_END(scmp8RR)
INST_BEGIN(scmp16RR) { compareRR<i16>(opPtr, regPtr, cmpFlags); }
INST_END(scmp16RR)
INST_BEGIN(scmp32RR) { compareRR<i32>(opPtr, regPtr, cmpFlags); }
INST_END(scmp32RR)
INST_BEGIN(scmp64RR) { compareRR<i64>(opPtr, regPtr, cmpFlags); }
INST_END(scmp64RR)

INST_BEGIN(ucmp8RV) { compareRV<u8>(opPtr, regPtr, cmpFlags); }
INST_END(ucmp8RV)
INST_BEGIN(ucmp16RV) { compareRV<u16>(opPtr, regPtr, cmpFlags); }
INST_END(ucmp16RV)
INST_BEGIN(ucmp32RV) { compareRV<u32>(opPtr, regPtr, cmpFlags); }
INST_END(ucmp32RV)
INST_BEGIN(ucmp64RV) { compareRV<u64>(opPtr, regPtr, cmpFlags); }
INST_END(ucmp64RV)

INST_BEGIN(scmp8RV) { compareRV<i8>(opPtr, regPtr, cmpFlags); }
INST_END(scmp8RV)
INST_BEGIN(scmp16RV) { compareRV<i16>(opPtr, regPtr, cmpFlags); }
INST_END(scmp16RV)
INST_BEGIN(scmp32RV) { compareRV<i32>(opPtr, regPtr, cmpFlags); }
INST_END(scmp32RV)
INST_BEGIN(scmp64RV) { compareRV<i64>(opPtr, regPtr, cmpFlags); }
INST_END(scmp64RV)

INST_BEGIN(fcmp32RR) { compareRR<f32>(opPtr, regPtr, cmpFlags); }
INST_END(fcmp32RR)
INST_BEGIN(fcmp64RR) { compareRR<f64>(opPtr, regPtr, cmpFlags); }
INST_END(fcmp64RR)
INST_BEGIN(fcmp32RV) { compareRV<f32>(opPtr, regPtr, cmpFlags); }
INST_END(fcmp32RV)
INST_BEGIN(fcmp64RV) { compareRV<f64>(opPtr, regPtr, cmpFlags); }
INST_END(fcmp64RV)

INST_BEGIN(stest8) { testR<i8>(opPtr, regPtr, cmpFlags); }
INST_END(stest8)
INST_BEGIN(stest16) { testR<i16>(opPtr, regPtr, cmpFlags); }
INST_END(stest16)
INST_BEGIN(stest32) { testR<i32>(opPtr, regPtr, cmpFlags); }
INST_END(stest32)
INST_BEGIN(stest64) { testR<i64>(opPtr, regPtr, cmpFlags); }
INST_END(stest64)

INST_BEGIN(utest8) { testR<u8>(opPtr, regPtr, cmpFlags); }
INST_END(utest8)
INST_BEGIN(utest16) { testR<u16>(opPtr, regPtr, cmpFlags); }
INST_END(utest16)
INST_BEGIN(utest32) { testR<u32>(opPtr, regPtr, cmpFlags); }
INST_END(utest32)
INST_BEGIN(utest64) { testR<u64>(opPtr, regPtr, cmpFlags); }
INST_END(utest64)

/// ## load comparison results
INST_BEGIN(sete) { set(opPtr, regPtr, equal(cmpFlags)); }
INST_END(sete)
INST_BEGIN(setne) { set(opPtr, regPtr, notEqual(cmpFlags)); }
INST_END(setne)
INST_BEGIN(setl) { set(opPtr, regPtr, less(cmpFlags)); }
INST_END(setl)
INST_BEGIN(setle) { set(opPtr, regPtr, lessEq(cmpFlags)); }
INST_END(setle)
INST_BEGIN(setg) { set(opPtr, regPtr, greater(cmpFlags)); }
INST_END(setg)
INST_BEGIN(setge) { set(opPtr, regPtr, greaterEq(cmpFlags)); }
INST_END(setge)

/// ## Unary operations
INST_BEGIN(lnt) { unaryR<u64>(opPtr, regPtr, LogNot); }
INST_END(lnt)
INST_BEGIN(bnt) { unaryR<u64>(opPtr, regPtr, BitNot); }
INST_END(bnt)
INST_BEGIN(neg8) { unaryR<i8>(opPtr, regPtr, Negate); }
INST_END(neg8)
INST_BEGIN(neg16) { unaryR<i16>(opPtr, regPtr, Negate); }
INST_END(neg16)
INST_BEGIN(neg32) { unaryR<i32>(opPtr, regPtr, Negate); }
INST_END(neg32)
INST_BEGIN(neg64) { unaryR<i64>(opPtr, regPtr, Negate); }
INST_END(neg64)

/// ## 64 bit integral arithmetic
INST_BEGIN(add64RR) { arithmeticRR<u64>(opPtr, regPtr, Add); }
INST_END(add64RR)
INST_BEGIN(add64RV) { arithmeticRV<u64>(opPtr, regPtr, Add); }
INST_END(add64RV)
INST_BEGIN(add64RM) { arithmeticRM<u64>(memory, opPtr, regPtr, Add); }
INST_END(add64RM)
INST_BEGIN(sub64RR) { arithmeticRR<u64>(opPtr, regPtr, Sub); }
INST_END(sub64RR)
INST_BEGIN(sub64RV) { arithmeticRV<u64>(opPtr, regPtr, Sub); }
INST_END(sub64RV)
INST_BEGIN(sub64RM) { arithmeticRM<u64>(memory, opPtr, regPtr, Sub); }
INST_END(sub64RM)
INST_BEGIN(mul64RR) { arithmeticRR<u64>(opPtr, regPtr, Mul); }
INST_END(mul64RR)
INST_BEGIN(mul64RV) { arithmeticRV<u64>(opPtr, regPtr, Mul); }
INST_END(mul64RV)
INST_BEGIN(mul64RM) { arithmeticRM<u64>(memory, opPtr, regPtr, Mul); }
INST_END(mul64RM)
INST_BEGIN(udiv64RR) { arithmeticRR<u64>(opPtr, regPtr, Div); }
INST_END(udiv64RR)
INST_BEGIN(udiv64RV) { arithmeticRV<u64>(opPtr, regPtr, Div); }
INST_END(udiv64RV)
INST_BEGIN(udiv64RM) { arithmeticRM<u64>(memory, opPtr, regPtr, Div); }
INST_END(udiv64RM)
INST_BEGIN(sdiv64RR) { arithmeticRR<i64>(opPtr, regPtr, Div); }
INST_END(sdiv64RR)
INST_BEGIN(sdiv64RV) { arithmeticRV<i64>(opPtr, regPtr, Div); }
INST_END(sdiv64RV)
INST_BEGIN(sdiv64RM) { arithmeticRM<i64>(memory, opPtr, regPtr, Div); }
INST_END(sdiv64RM)
INST_BEGIN(urem64RR) { arithmeticRR<u64>(opPtr, regPtr, Rem); }
INST_END(urem64RR)
INST_BEGIN(urem64RV) { arithmeticRV<u64>(opPtr, regPtr, Rem); }
INST_END(urem64RV)
INST_BEGIN(urem64RM) { arithmeticRM<u64>(memory, opPtr, regPtr, Rem); }
INST_END(urem64RM)
INST_BEGIN(srem64RR) { arithmeticRR<i64>(opPtr, regPtr, Rem); }
INST_END(srem64RR)
INST_BEGIN(srem64RV) { arithmeticRV<i64>(opPtr, regPtr, Rem); }
INST_END(srem64RV)
INST_BEGIN(srem64RM) { arithmeticRM<i64>(memory, opPtr, regPtr, Rem); }
INST_END(srem64RM)

/// ## 32 bit integral arithmetic
INST_BEGIN(add32RR) { arithmeticRR<u32>(opPtr, regPtr, Add); }
INST_END(add32RR)
INST_BEGIN(add32RV) { arithmeticRV<u32>(opPtr, regPtr, Add); }
INST_END(add32RV)
INST_BEGIN(add32RM) { arithmeticRM<u32>(memory, opPtr, regPtr, Add); }
INST_END(add32RM)
INST_BEGIN(sub32RR) { arithmeticRR<u32>(opPtr, regPtr, Sub); }
INST_END(sub32RR)
INST_BEGIN(sub32RV) { arithmeticRV<u32>(opPtr, regPtr, Sub); }
INST_END(sub32RV)
INST_BEGIN(sub32RM) { arithmeticRM<u32>(memory, opPtr, regPtr, Sub); }
INST_END(sub32RM)
INST_BEGIN(mul32RR) { arithmeticRR<u32>(opPtr, regPtr, Mul); }
INST_END(mul32RR)
INST_BEGIN(mul32RV) { arithmeticRV<u32>(opPtr, regPtr, Mul); }
INST_END(mul32RV)
INST_BEGIN(mul32RM) { arithmeticRM<u32>(memory, opPtr, regPtr, Mul); }
INST_END(mul32RM)
INST_BEGIN(udiv32RR) { arithmeticRR<u32>(opPtr, regPtr, Div); }
INST_END(udiv32RR)
INST_BEGIN(udiv32RV) { arithmeticRV<u32>(opPtr, regPtr, Div); }
INST_END(udiv32RV)
INST_BEGIN(udiv32RM) { arithmeticRM<u32>(memory, opPtr, regPtr, Div); }
INST_END(udiv32RM)
INST_BEGIN(sdiv32RR) { arithmeticRR<i32>(opPtr, regPtr, Div); }
INST_END(sdiv32RR)
INST_BEGIN(sdiv32RV) { arithmeticRV<i32>(opPtr, regPtr, Div); }
INST_END(sdiv32RV)
INST_BEGIN(sdiv32RM) { arithmeticRM<i32>(memory, opPtr, regPtr, Div); }
INST_END(sdiv32RM)
INST_BEGIN(urem32RR) { arithmeticRR<u32>(opPtr, regPtr, Rem); }
INST_END(urem32RR)
INST_BEGIN(urem32RV) { arithmeticRV<u32>(opPtr, regPtr, Rem); }
INST_END(urem32RV)
INST_BEGIN(urem32RM) { arithmeticRM<u32>(memory, opPtr, regPtr, Rem); }
INST_END(urem32RM)
INST_BEGIN(srem32RR) { arithmeticRR<i32>(opPtr, regPtr, Rem); }
INST_END(srem32RR)
INST_BEGIN(srem32RV) { arithmeticRV<i32>(opPtr, regPtr, Rem); }
INST_END(srem32RV)
INST_BEGIN(srem32RM) { arithmeticRM<i32>(memory, opPtr, regPtr, Rem); }
INST_END(srem32RM)

/// ## 64 bit Floating point arithmetic
INST_BEGIN(fadd64RR) { arithmeticRR<f64>(opPtr, regPtr, Add); }
INST_END(fadd64RR)
INST_BEGIN(fadd64RV) { arithmeticRV<f64>(opPtr, regPtr, Add); }
INST_END(fadd64RV)
INST_BEGIN(fadd64RM) { arithmeticRM<f64>(memory, opPtr, regPtr, Add); }
INST_END(fadd64RM)
INST_BEGIN(fsub64RR) { arithmeticRR<f64>(opPtr, regPtr, Sub); }
INST_END(fsub64RR)
INST_BEGIN(fsub64RV) { arithmeticRV<f64>(opPtr, regPtr, Sub); }
INST_END(fsub64RV)
INST_BEGIN(fsub64RM) { arithmeticRM<f64>(memory, opPtr, regPtr, Sub); }
INST_END(fsub64RM)
INST_BEGIN(fmul64RR) { arithmeticRR<f64>(opPtr, regPtr, Mul); }
INST_END(fmul64RR)
INST_BEGIN(fmul64RV) { arithmeticRV<f64>(opPtr, regPtr, Mul); }
INST_END(fmul64RV)
INST_BEGIN(fmul64RM) { arithmeticRM<f64>(memory, opPtr, regPtr, Mul); }
INST_END(fmul64RM)
INST_BEGIN(fdiv64RR) { arithmeticRR<f64>(opPtr, regPtr, Div); }
INST_END(fdiv64RR)
INST_BEGIN(fdiv64RV) { arithmeticRV<f64>(opPtr, regPtr, Div); }
INST_END(fdiv64RV)
INST_BEGIN(fdiv64RM) { arithmeticRM<f64>(memory, opPtr, regPtr, Div); }
INST_END(fdiv64RM)

/// ## 32 bit Floating point arithmetic
INST_BEGIN(fadd32RR) { arithmeticRR<f32>(opPtr, regPtr, Add); }
INST_END(fadd32RR)
INST_BEGIN(fadd32RV) { arithmeticRV<f32>(opPtr, regPtr, Add); }
INST_END(fadd32RV)
INST_BEGIN(fadd32RM) { arithmeticRM<f32>(memory, opPtr, regPtr, Add); }
INST_END(fadd32RM)
INST_BEGIN(fsub32RR) { arithmeticRR<f32>(opPtr, regPtr, Sub); }
INST_END(fsub32RR)
INST_BEGIN(fsub32RV) { arithmeticRV<f32>(opPtr, regPtr, Sub); }
INST_END(fsub32RV)
INST_BEGIN(fsub32RM) { arithmeticRM<f32>(memory, opPtr, regPtr, Sub); }
INST_END(fsub32RM)
INST_BEGIN(fmul32RR) { arithmeticRR<f32>(opPtr, regPtr, Mul); }
INST_END(fmul32RR)
INST_BEGIN(fmul32RV) { arithmeticRV<f32>(opPtr, regPtr, Mul); }
INST_END(fmul32RV)
INST_BEGIN(fmul32RM) { arithmeticRM<f32>(memory, opPtr, regPtr, Mul); }
INST_END(fmul32RM)
INST_BEGIN(fdiv32RR) { arithmeticRR<f32>(opPtr, regPtr, Div); }
INST_END(fdiv32RR)
INST_BEGIN(fdiv32RV) { arithmeticRV<f32>(opPtr, regPtr, Div); }
INST_END(fdiv32RV)
INST_BEGIN(fdiv32RM) { arithmeticRM<f32>(memory, opPtr, regPtr, Div); }
INST_END(fdiv32RM)

/// ## 64 bit logical shifts
INST_BEGIN(lsl64RR) { arithmeticRR<u64>(opPtr, regPtr, LSH); }
INST_END(lsl64RR)
INST_BEGIN(lsl64RV) { arithmeticRV<u64, u8>(opPtr, regPtr, LSH); }
INST_END(lsl64RV)
INST_BEGIN(lsl64RM) { arithmeticRM<u64>(memory, opPtr, regPtr, LSH); }
INST_END(lsl64RM)
INST_BEGIN(lsr64RR) { arithmeticRR<u64>(opPtr, regPtr, RSH); }
INST_END(lsr64RR)
INST_BEGIN(lsr64RV) { arithmeticRV<u64, u8>(opPtr, regPtr, RSH); }
INST_END(lsr64RV)
INST_BEGIN(lsr64RM) { arithmeticRM<u64>(memory, opPtr, regPtr, RSH); }
INST_END(lsr64RM)

/// ## 32 bit logical shifts
INST_BEGIN(lsl32RR) { arithmeticRR<u32>(opPtr, regPtr, LSH); }
INST_END(lsl32RR)
INST_BEGIN(lsl32RV) { arithmeticRV<u32, u8>(opPtr, regPtr, LSH); }
INST_END(lsl32RV)
INST_BEGIN(lsl32RM) { arithmeticRM<u32>(memory, opPtr, regPtr, LSH); }
INST_END(lsl32RM)
INST_BEGIN(lsr32RR) { arithmeticRR<u32>(opPtr, regPtr, RSH); }
INST_END(lsr32RR)
INST_BEGIN(lsr32RV) { arithmeticRV<u32, u8>(opPtr, regPtr, RSH); }
INST_END(lsr32RV)
INST_BEGIN(lsr32RM) { arithmeticRM<u32>(memory, opPtr, regPtr, RSH); }
INST_END(lsr32RM)

/// ## 64 bit arithmetic shifts
INST_BEGIN(asl64RR) { arithmeticRR<u64>(opPtr, regPtr, ALSH); }
INST_END(asl64RR)
INST_BEGIN(asl64RV) { arithmeticRV<u64, u8>(opPtr, regPtr, ALSH); }
INST_END(asl64RV)
INST_BEGIN(asl64RM) { arithmeticRM<u64>(memory, opPtr, regPtr, ALSH); }
INST_END(asl64RM)
INST_BEGIN(asr64RR) { arithmeticRR<u64>(opPtr, regPtr, ARSH); }
INST_END(asr64RR)
INST_BEGIN(asr64RV) { arithmeticRV<u64, u8>(opPtr, regPtr, ARSH); }
INST_END(asr64RV)
INST_BEGIN(asr64RM) { arithmeticRM<u64>(memory, opPtr, regPtr, ARSH); }
INST_END(asr64RM)

/// ## 32 bit arithmetic shifts
INST_BEGIN(asl32RR) { arithmeticRR<u32>(opPtr, regPtr, ALSH); }
INST_END(asl32RR)
INST_BEGIN(asl32RV) { arithmeticRV<u32, u8>(opPtr, regPtr, ALSH); }
INST_END(asl32RV)
INST_BEGIN(asl32RM) { arithmeticRM<u32>(memory, opPtr, regPtr, ALSH); }
INST_END(asl32RM)
INST_BEGIN(asr32RR) { arithmeticRR<u32>(opPtr, regPtr, ARSH); }
INST_END(asr32RR)
INST_BEGIN(asr32RV) { arithmeticRV<u32, u8>(opPtr, regPtr, ARSH); }
INST_END(asr32RV)
INST_BEGIN(asr32RM) { arithmeticRM<u32>(memory, opPtr, regPtr, ARSH); }
INST_END(asr32RM)

/// ## 64 bit bitwise operations
INST_BEGIN(and64RR) { arithmeticRR<u64>(opPtr, regPtr, BitAnd); }
INST_END(and64RR)
INST_BEGIN(and64RV) { arithmeticRV<u64>(opPtr, regPtr, BitAnd); }
INST_END(and64RV)
INST_BEGIN(and64RM) { arithmeticRM<u64>(memory, opPtr, regPtr, BitAnd); }
INST_END(and64RM)
INST_BEGIN(or64RR) { arithmeticRR<u64>(opPtr, regPtr, BitOr); }
INST_END(or64RR)
INST_BEGIN(or64RV) { arithmeticRV<u64>(opPtr, regPtr, BitOr); }
INST_END(or64RV)
INST_BEGIN(or64RM) { arithmeticRM<u64>(memory, opPtr, regPtr, BitOr); }
INST_END(or64RM)
INST_BEGIN(xor64RR) { arithmeticRR<u64>(opPtr, regPtr, BitXOr); }
INST_END(xor64RR)
INST_BEGIN(xor64RV) { arithmeticRV<u64>(opPtr, regPtr, BitXOr); }
INST_END(xor64RV)
INST_BEGIN(xor64RM) { arithmeticRM<u64>(memory, opPtr, regPtr, BitXOr); }
INST_END(xor64RM)

/// ## 32 bit bitwise operations
INST_BEGIN(and32RR) { arithmeticRR<u32>(opPtr, regPtr, BitAnd); }
INST_END(and32RR)
INST_BEGIN(and32RV) { arithmeticRV<u32>(opPtr, regPtr, BitAnd); }
INST_END(and32RV)
INST_BEGIN(and32RM) { arithmeticRM<u32>(memory, opPtr, regPtr, BitAnd); }
INST_END(and32RM)
INST_BEGIN(or32RR) { arithmeticRR<u32>(opPtr, regPtr, BitOr); }
INST_END(or32RR)
INST_BEGIN(or32RV) { arithmeticRV<u32>(opPtr, regPtr, BitOr); }
INST_END(or32RV)
INST_BEGIN(or32RM) { arithmeticRM<u32>(memory, opPtr, regPtr, BitOr); }
INST_END(or32RM)
INST_BEGIN(xor32RR) { arithmeticRR<u32>(opPtr, regPtr, BitXOr); }
INST_END(xor32RR)
INST_BEGIN(xor32RV) { arithmeticRV<u32>(opPtr, regPtr, BitXOr); }
INST_END(xor32RV)
INST_BEGIN(xor32RM) { arithmeticRM<u32>(memory, opPtr, regPtr, BitXOr); }
INST_END(xor32RM)

/// ## Conversion
INST_BEGIN(sext1) { ::sext1(opPtr, regPtr); }
INST_END(sext1)
INST_BEGIN(sext8) { convert<i8, i64>(opPtr, regPtr); }
INST_END(sext8)
INST_BEGIN(sext16) { convert<i16, i64>(opPtr, regPtr); }
INST_END(sext16)
INST_BEGIN(sext32) { convert<i32, i64>(opPtr, regPtr); }
INST_END(sext32)
INST_BEGIN(fext) { convert<f32, f64>(opPtr, regPtr); }
INST_END(fext)
INST_BEGIN(ftrunc) { convert<f64, f32>(opPtr, regPtr); }
INST_END(ftrunc)

INST_BEGIN(s8tof32) { convert<i8, f32>(opPtr, regPtr); }
INST_END(s8tof32)
INST_BEGIN(s16tof32) { convert<i16, f32>(opPtr, regPtr); }
INST_END(s16tof32)
INST_BEGIN(s32tof32) { convert<i32, f32>(opPtr, regPtr); }
INST_END(s32tof32)
INST_BEGIN(s64tof32) { convert<i64, f32>(opPtr, regPtr); }
INST_END(s64tof32)
INST_BEGIN(u8tof32) { convert<u8, f32>(opPtr, regPtr); }
INST_END(u8tof32)
INST_BEGIN(u16tof32) { convert<u16, f32>(opPtr, regPtr); }
INST_END(u16tof32)
INST_BEGIN(u32tof32) { convert<u32, f32>(opPtr, regPtr); }
INST_END(u32tof32)
INST_BEGIN(u64tof32) { convert<u64, f32>(opPtr, regPtr); }
INST_END(u64tof32)
INST_BEGIN(s8tof64) { convert<i8, f64>(opPtr, regPtr); }
INST_END(s8tof64)
INST_BEGIN(s16tof64) { convert<i16, f64>(opPtr, regPtr); }
INST_END(s16tof64)
INST_BEGIN(s32tof64) { convert<i32, f64>(opPtr, regPtr); }
INST_END(s32tof64)
INST_BEGIN(s64tof64) { convert<i64, f64>(opPtr, regPtr); }
INST_END(s64tof64)
INST_BEGIN(u8tof64) { convert<u8, f64>(opPtr, regPtr); }
INST_END(u8tof64)
INST_BEGIN(u16tof64) { convert<u16, f64>(opPtr, regPtr); }
INST_END(u16tof64)
INST_BEGIN(u32tof64) { convert<u32, f64>(opPtr, regPtr); }
INST_END(u32tof64)
INST_BEGIN(u64tof64) { convert<u64, f64>(opPtr, regPtr); }
INST_END(u64tof64)

INST_BEGIN(f32tos8) { convert<f32, i8>(opPtr, regPtr); }
INST_END(f32tos8)
INST_BEGIN(f32tos16) { convert<f32, i16>(opPtr, regPtr); }
INST_END(f32tos16)
INST_BEGIN(f32tos32) { convert<f32, i32>(opPtr, regPtr); }
INST_END(f32tos32)
INST_BEGIN(f32tos64) { convert<f32, i64>(opPtr, regPtr); }
INST_END(f32tos64)
INST_BEGIN(f32tou8) { convert<f32, u8>(opPtr, regPtr); }
INST_END(f32tou8)
INST_BEGIN(f32tou16) { convert<f32, u16>(opPtr, regPtr); }
INST_END(f32tou16)
INST_BEGIN(f32tou32) { convert<f32, u32>(opPtr, regPtr); }
INST_END(f32tou32)
INST_BEGIN(f32tou64) { convert<f32, u64>(opPtr, regPtr); }
INST_END(f32tou64)
INST_BEGIN(f64tos8) { convert<f64, i8>(opPtr, regPtr); }
INST_END(f64tos8)
INST_BEGIN(f64tos16) { convert<f64, i16>(opPtr, regPtr); }
INST_END(f64tos16)
INST_BEGIN(f64tos32) { convert<f64, i32>(opPtr, regPtr); }
INST_END(f64tos32)
INST_BEGIN(f64tos64) { convert<f64, i64>(opPtr, regPtr); }
INST_END(f64tos64)
INST_BEGIN(f64tou8) { convert<f64, u8>(opPtr, regPtr); }
INST_END(f64tou8)
INST_BEGIN(f64tou16) { convert<f64, u16>(opPtr, regPtr); }
INST_END(f64tou16)
INST_BEGIN(f64tou32) { convert<f64, u32>(opPtr, regPtr); }
INST_END(f64tou32)
INST_BEGIN(f64tou64) { convert<f64, u64>(opPtr, regPtr); }
INST_END(f64tou64)

#undef INST_BEGIN
#undef INST_END
#undef TERMINATE_EXECUTION
