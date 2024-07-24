// No include guard, because this file may be included multiple times

#ifndef INST
#error INST(opcode) macro must be defined
#define INST(...) // Define here to avoid further errors
#endif

#ifndef TERMINATE_EXECUTION
#error TERMINATE_EXECUTION() macro must be defined
#define TERMINATE_EXECUTION() // Define here to avoid further errors
#endif

INST(call) {
    performCall<OpCode::call>(memory, opPtr, binary, iptr, regPtr,
                              currentFrame.stackPtr);
}
INST(icallr) {
    performCall<OpCode::icallr>(memory, opPtr, binary, iptr, regPtr,
                                currentFrame.stackPtr);
}
INST(icallm) {
    performCall<OpCode::icallm>(memory, opPtr, binary, iptr, regPtr,
                                currentFrame.stackPtr);
}

INST(ret) {
    if UTL_UNLIKELY (currentFrame.bottomReg == regPtr) {
        /// Meaning we are the root of the call tree aka. the main/start
        /// function, so we set the instruction pointer to the program
        /// break to terminate execution.
        TERMINATE_EXECUTION();
    }
    else {
        iptr = utl::bit_cast<u8 const*>(regPtr[-1]);
        currentFrame.stackPtr = utl::bit_cast<VirtualPointer>(regPtr[-3]);
        regPtr -= regPtr[-2];
    }
}

INST(cfng) {
    size_t regPtrOffset = opPtr[0];
    size_t index = load<u16>(&opPtr[1]);
    auto& function = foreignFunctionTable[index];
    invokeFFI(function, regPtr + regPtrOffset, memory);
}

INST(cbltn) {
    size_t regPtrOffset = opPtr[0];
    size_t index = load<u16>(&opPtr[1]);
    try {
        builtinFunctionTable[index].invoke(regPtr + regPtrOffset, parent);
    }
    catch (ExitException const&) {
        TERMINATE_EXECUTION();
    }
}

INST(terminate) { TERMINATE_EXECUTION(); }

/// ## Loads and storeRegs
INST(mov64RR) {
    size_t destRegIdx = opPtr[0];
    size_t sourceRegIdx = opPtr[1];
    regPtr[destRegIdx] = regPtr[sourceRegIdx];
}

INST(mov64RV) {
    size_t destRegIdx = opPtr[0];
    regPtr[destRegIdx] = load<u64>(opPtr + 1);
}

INST(mov8MR) { moveMR<1>(memory, opPtr, regPtr); }
INST(mov16MR) { moveMR<2>(memory, opPtr, regPtr); }
INST(mov32MR) { moveMR<4>(memory, opPtr, regPtr); }
INST(mov64MR) { moveMR<8>(memory, opPtr, regPtr); }
INST(mov8RM) { moveRM<1>(memory, opPtr, regPtr); }
INST(mov16RM) { moveRM<2>(memory, opPtr, regPtr); }
INST(mov32RM) { moveRM<4>(memory, opPtr, regPtr); }
INST(mov64RM) { moveRM<8>(memory, opPtr, regPtr); }

/// ## Conditional moves
INST(cmove64RR) { condMove64RR(opPtr, regPtr, equal(cmpFlags)); }
INST(cmove64RV) { condMove64RV(opPtr, regPtr, equal(cmpFlags)); }
INST(cmove8RM) { condMoveRM<1>(memory, opPtr, regPtr, equal(cmpFlags)); }
INST(cmove16RM) { condMoveRM<2>(memory, opPtr, regPtr, equal(cmpFlags)); }
INST(cmove32RM) { condMoveRM<4>(memory, opPtr, regPtr, equal(cmpFlags)); }
INST(cmove64RM) { condMoveRM<8>(memory, opPtr, regPtr, equal(cmpFlags)); }

INST(cmovne64RR) { condMove64RR(opPtr, regPtr, notEqual(cmpFlags)); }
INST(cmovne64RV) { condMove64RV(opPtr, regPtr, notEqual(cmpFlags)); }
INST(cmovne8RM) { condMoveRM<1>(memory, opPtr, regPtr, notEqual(cmpFlags)); }
INST(cmovne16RM) { condMoveRM<2>(memory, opPtr, regPtr, notEqual(cmpFlags)); }
INST(cmovne32RM) { condMoveRM<4>(memory, opPtr, regPtr, notEqual(cmpFlags)); }
INST(cmovne64RM) { condMoveRM<8>(memory, opPtr, regPtr, notEqual(cmpFlags)); }

INST(cmovl64RR) { condMove64RR(opPtr, regPtr, less(cmpFlags)); }
INST(cmovl64RV) { condMove64RV(opPtr, regPtr, less(cmpFlags)); }
INST(cmovl8RM) { condMoveRM<1>(memory, opPtr, regPtr, less(cmpFlags)); }
INST(cmovl16RM) { condMoveRM<2>(memory, opPtr, regPtr, less(cmpFlags)); }
INST(cmovl32RM) { condMoveRM<4>(memory, opPtr, regPtr, less(cmpFlags)); }
INST(cmovl64RM) { condMoveRM<8>(memory, opPtr, regPtr, less(cmpFlags)); }

INST(cmovle64RR) { condMove64RR(opPtr, regPtr, lessEq(cmpFlags)); }
INST(cmovle64RV) { condMove64RV(opPtr, regPtr, lessEq(cmpFlags)); }
INST(cmovle8RM) { condMoveRM<1>(memory, opPtr, regPtr, lessEq(cmpFlags)); }
INST(cmovle16RM) { condMoveRM<2>(memory, opPtr, regPtr, lessEq(cmpFlags)); }
INST(cmovle32RM) { condMoveRM<4>(memory, opPtr, regPtr, lessEq(cmpFlags)); }
INST(cmovle64RM) { condMoveRM<8>(memory, opPtr, regPtr, lessEq(cmpFlags)); }

INST(cmovg64RR) { condMove64RR(opPtr, regPtr, greater(cmpFlags)); }
INST(cmovg64RV) { condMove64RV(opPtr, regPtr, greater(cmpFlags)); }
INST(cmovg8RM) { condMoveRM<1>(memory, opPtr, regPtr, greater(cmpFlags)); }
INST(cmovg16RM) { condMoveRM<2>(memory, opPtr, regPtr, greater(cmpFlags)); }
INST(cmovg32RM) { condMoveRM<4>(memory, opPtr, regPtr, greater(cmpFlags)); }
INST(cmovg64RM) { condMoveRM<8>(memory, opPtr, regPtr, greater(cmpFlags)); }

INST(cmovge64RR) { condMove64RR(opPtr, regPtr, greaterEq(cmpFlags)); }
INST(cmovge64RV) { condMove64RV(opPtr, regPtr, greaterEq(cmpFlags)); }
INST(cmovge8RM) { condMoveRM<1>(memory, opPtr, regPtr, greaterEq(cmpFlags)); }
INST(cmovge16RM) { condMoveRM<2>(memory, opPtr, regPtr, greaterEq(cmpFlags)); }
INST(cmovge32RM) { condMoveRM<4>(memory, opPtr, regPtr, greaterEq(cmpFlags)); }
INST(cmovge64RM) { condMoveRM<8>(memory, opPtr, regPtr, greaterEq(cmpFlags)); }

/// ## Stack pointer manipulation
INST(lincsp) {
    size_t destRegIdx = load<u8>(opPtr);
    size_t offset = load<u16>(opPtr + 1);
    if (SVM_UNLIKELY(offset % 8 != 0)) {
        throwError<InvalidStackAllocationError>(offset);
    }
    regPtr[destRegIdx] = utl::bit_cast<u64>(currentFrame.stackPtr);
    currentFrame.stackPtr += offset;
}

/// ## Address calculation
INST(lea) {
    size_t destRegIdx = load<u8>(opPtr);
    VirtualPointer ptr = getPointer(regPtr, opPtr + 1);
    regPtr[destRegIdx] = utl::bit_cast<u64>(ptr);
}

/// ## Jumps
INST(jmp) { jump<OpCode::jmp>(opPtr, binary, iptr, true); }
INST(je) { jump<OpCode::je>(opPtr, binary, iptr, equal(cmpFlags)); }
INST(jne) { jump<OpCode::jne>(opPtr, binary, iptr, notEqual(cmpFlags)); }
INST(jl) { jump<OpCode::jl>(opPtr, binary, iptr, less(cmpFlags)); }
INST(jle) { jump<OpCode::jle>(opPtr, binary, iptr, lessEq(cmpFlags)); }
INST(jg) { jump<OpCode::jg>(opPtr, binary, iptr, greater(cmpFlags)); }
INST(jge) { jump<OpCode::jge>(opPtr, binary, iptr, greaterEq(cmpFlags)); }

/// ## Comparison
INST(ucmp8RR) { compareRR<u8>(opPtr, regPtr, cmpFlags); }
INST(ucmp16RR) { compareRR<u16>(opPtr, regPtr, cmpFlags); }
INST(ucmp32RR) { compareRR<u32>(opPtr, regPtr, cmpFlags); }
INST(ucmp64RR) { compareRR<u64>(opPtr, regPtr, cmpFlags); }

INST(scmp8RR) { compareRR<i8>(opPtr, regPtr, cmpFlags); }
INST(scmp16RR) { compareRR<i16>(opPtr, regPtr, cmpFlags); }
INST(scmp32RR) { compareRR<i32>(opPtr, regPtr, cmpFlags); }
INST(scmp64RR) { compareRR<i64>(opPtr, regPtr, cmpFlags); }

INST(ucmp8RV) { compareRV<u8>(opPtr, regPtr, cmpFlags); }
INST(ucmp16RV) { compareRV<u16>(opPtr, regPtr, cmpFlags); }
INST(ucmp32RV) { compareRV<u32>(opPtr, regPtr, cmpFlags); }
INST(ucmp64RV) { compareRV<u64>(opPtr, regPtr, cmpFlags); }

INST(scmp8RV) { compareRV<i8>(opPtr, regPtr, cmpFlags); }
INST(scmp16RV) { compareRV<i16>(opPtr, regPtr, cmpFlags); }
INST(scmp32RV) { compareRV<i32>(opPtr, regPtr, cmpFlags); }
INST(scmp64RV) { compareRV<i64>(opPtr, regPtr, cmpFlags); }

INST(fcmp32RR) { compareRR<f32>(opPtr, regPtr, cmpFlags); }
INST(fcmp64RR) { compareRR<f64>(opPtr, regPtr, cmpFlags); }
INST(fcmp32RV) { compareRV<f32>(opPtr, regPtr, cmpFlags); }
INST(fcmp64RV) { compareRV<f64>(opPtr, regPtr, cmpFlags); }

INST(stest8) { testR<i8>(opPtr, regPtr, cmpFlags); }
INST(stest16) { testR<i16>(opPtr, regPtr, cmpFlags); }
INST(stest32) { testR<i32>(opPtr, regPtr, cmpFlags); }
INST(stest64) { testR<i64>(opPtr, regPtr, cmpFlags); }

INST(utest8) { testR<u8>(opPtr, regPtr, cmpFlags); }
INST(utest16) { testR<u16>(opPtr, regPtr, cmpFlags); }
INST(utest32) { testR<u32>(opPtr, regPtr, cmpFlags); }
INST(utest64) { testR<u64>(opPtr, regPtr, cmpFlags); }

/// ## load comparison results
INST(sete) { set(opPtr, regPtr, equal(cmpFlags)); }
INST(setne) { set(opPtr, regPtr, notEqual(cmpFlags)); }
INST(setl) { set(opPtr, regPtr, less(cmpFlags)); }
INST(setle) { set(opPtr, regPtr, lessEq(cmpFlags)); }
INST(setg) { set(opPtr, regPtr, greater(cmpFlags)); }
INST(setge) { set(opPtr, regPtr, greaterEq(cmpFlags)); }

/// ## Unary operations
INST(lnt) { unaryR<u64>(opPtr, regPtr, LogNot); }
INST(bnt) { unaryR<u64>(opPtr, regPtr, BitNot); }
INST(neg8) { unaryR<i8>(opPtr, regPtr, Negate); }
INST(neg16) { unaryR<i16>(opPtr, regPtr, Negate); }
INST(neg32) { unaryR<i32>(opPtr, regPtr, Negate); }
INST(neg64) { unaryR<i64>(opPtr, regPtr, Negate); }

/// ## 64 bit integral arithmetic
INST(add64RR) { arithmeticRR<u64>(opPtr, regPtr, Add); }
INST(add64RV) { arithmeticRV<u64>(opPtr, regPtr, Add); }
INST(add64RM) { arithmeticRM<u64>(memory, opPtr, regPtr, Add); }
INST(sub64RR) { arithmeticRR<u64>(opPtr, regPtr, Sub); }
INST(sub64RV) { arithmeticRV<u64>(opPtr, regPtr, Sub); }
INST(sub64RM) { arithmeticRM<u64>(memory, opPtr, regPtr, Sub); }
INST(mul64RR) { arithmeticRR<u64>(opPtr, regPtr, Mul); }
INST(mul64RV) { arithmeticRV<u64>(opPtr, regPtr, Mul); }
INST(mul64RM) { arithmeticRM<u64>(memory, opPtr, regPtr, Mul); }
INST(udiv64RR) { arithmeticRR<u64>(opPtr, regPtr, Div); }
INST(udiv64RV) { arithmeticRV<u64>(opPtr, regPtr, Div); }
INST(udiv64RM) { arithmeticRM<u64>(memory, opPtr, regPtr, Div); }
INST(sdiv64RR) { arithmeticRR<i64>(opPtr, regPtr, Div); }
INST(sdiv64RV) { arithmeticRV<i64>(opPtr, regPtr, Div); }
INST(sdiv64RM) { arithmeticRM<i64>(memory, opPtr, regPtr, Div); }
INST(urem64RR) { arithmeticRR<u64>(opPtr, regPtr, Rem); }
INST(urem64RV) { arithmeticRV<u64>(opPtr, regPtr, Rem); }
INST(urem64RM) { arithmeticRM<u64>(memory, opPtr, regPtr, Rem); }
INST(srem64RR) { arithmeticRR<i64>(opPtr, regPtr, Rem); }
INST(srem64RV) { arithmeticRV<i64>(opPtr, regPtr, Rem); }
INST(srem64RM) { arithmeticRM<i64>(memory, opPtr, regPtr, Rem); }

/// ## 32 bit integral arithmetic
INST(add32RR) { arithmeticRR<u32>(opPtr, regPtr, Add); }
INST(add32RV) { arithmeticRV<u32>(opPtr, regPtr, Add); }
INST(add32RM) { arithmeticRM<u32>(memory, opPtr, regPtr, Add); }
INST(sub32RR) { arithmeticRR<u32>(opPtr, regPtr, Sub); }
INST(sub32RV) { arithmeticRV<u32>(opPtr, regPtr, Sub); }
INST(sub32RM) { arithmeticRM<u32>(memory, opPtr, regPtr, Sub); }
INST(mul32RR) { arithmeticRR<u32>(opPtr, regPtr, Mul); }
INST(mul32RV) { arithmeticRV<u32>(opPtr, regPtr, Mul); }
INST(mul32RM) { arithmeticRM<u32>(memory, opPtr, regPtr, Mul); }
INST(udiv32RR) { arithmeticRR<u32>(opPtr, regPtr, Div); }
INST(udiv32RV) { arithmeticRV<u32>(opPtr, regPtr, Div); }
INST(udiv32RM) { arithmeticRM<u32>(memory, opPtr, regPtr, Div); }
INST(sdiv32RR) { arithmeticRR<i32>(opPtr, regPtr, Div); }
INST(sdiv32RV) { arithmeticRV<i32>(opPtr, regPtr, Div); }
INST(sdiv32RM) { arithmeticRM<i32>(memory, opPtr, regPtr, Div); }
INST(urem32RR) { arithmeticRR<u32>(opPtr, regPtr, Rem); }
INST(urem32RV) { arithmeticRV<u32>(opPtr, regPtr, Rem); }
INST(urem32RM) { arithmeticRM<u32>(memory, opPtr, regPtr, Rem); }
INST(srem32RR) { arithmeticRR<i32>(opPtr, regPtr, Rem); }
INST(srem32RV) { arithmeticRV<i32>(opPtr, regPtr, Rem); }
INST(srem32RM) { arithmeticRM<i32>(memory, opPtr, regPtr, Rem); }

/// ## 64 bit Floating point arithmetic
INST(fadd64RR) { arithmeticRR<f64>(opPtr, regPtr, Add); }
INST(fadd64RV) { arithmeticRV<f64>(opPtr, regPtr, Add); }
INST(fadd64RM) { arithmeticRM<f64>(memory, opPtr, regPtr, Add); }
INST(fsub64RR) { arithmeticRR<f64>(opPtr, regPtr, Sub); }
INST(fsub64RV) { arithmeticRV<f64>(opPtr, regPtr, Sub); }
INST(fsub64RM) { arithmeticRM<f64>(memory, opPtr, regPtr, Sub); }
INST(fmul64RR) { arithmeticRR<f64>(opPtr, regPtr, Mul); }
INST(fmul64RV) { arithmeticRV<f64>(opPtr, regPtr, Mul); }
INST(fmul64RM) { arithmeticRM<f64>(memory, opPtr, regPtr, Mul); }
INST(fdiv64RR) { arithmeticRR<f64>(opPtr, regPtr, Div); }
INST(fdiv64RV) { arithmeticRV<f64>(opPtr, regPtr, Div); }
INST(fdiv64RM) { arithmeticRM<f64>(memory, opPtr, regPtr, Div); }

/// ## 32 bit Floating point arithmetic
INST(fadd32RR) { arithmeticRR<f32>(opPtr, regPtr, Add); }
INST(fadd32RV) { arithmeticRV<f32>(opPtr, regPtr, Add); }
INST(fadd32RM) { arithmeticRM<f32>(memory, opPtr, regPtr, Add); }
INST(fsub32RR) { arithmeticRR<f32>(opPtr, regPtr, Sub); }
INST(fsub32RV) { arithmeticRV<f32>(opPtr, regPtr, Sub); }
INST(fsub32RM) { arithmeticRM<f32>(memory, opPtr, regPtr, Sub); }
INST(fmul32RR) { arithmeticRR<f32>(opPtr, regPtr, Mul); }
INST(fmul32RV) { arithmeticRV<f32>(opPtr, regPtr, Mul); }
INST(fmul32RM) { arithmeticRM<f32>(memory, opPtr, regPtr, Mul); }
INST(fdiv32RR) { arithmeticRR<f32>(opPtr, regPtr, Div); }
INST(fdiv32RV) { arithmeticRV<f32>(opPtr, regPtr, Div); }
INST(fdiv32RM) { arithmeticRM<f32>(memory, opPtr, regPtr, Div); }

/// ## 64 bit logical shifts
INST(lsl64RR) { arithmeticRR<u64>(opPtr, regPtr, LSH); }
INST(lsl64RV) { arithmeticRV<u64, u8>(opPtr, regPtr, LSH); }
INST(lsl64RM) { arithmeticRM<u64>(memory, opPtr, regPtr, LSH); }
INST(lsr64RR) { arithmeticRR<u64>(opPtr, regPtr, RSH); }
INST(lsr64RV) { arithmeticRV<u64, u8>(opPtr, regPtr, RSH); }
INST(lsr64RM) { arithmeticRM<u64>(memory, opPtr, regPtr, RSH); }

/// ## 32 bit logical shifts
INST(lsl32RR) { arithmeticRR<u32>(opPtr, regPtr, LSH); }
INST(lsl32RV) { arithmeticRV<u32, u8>(opPtr, regPtr, LSH); }
INST(lsl32RM) { arithmeticRM<u32>(memory, opPtr, regPtr, LSH); }
INST(lsr32RR) { arithmeticRR<u32>(opPtr, regPtr, RSH); }
INST(lsr32RV) { arithmeticRV<u32, u8>(opPtr, regPtr, RSH); }
INST(lsr32RM) { arithmeticRM<u32>(memory, opPtr, regPtr, RSH); }

/// ## 64 bit arithmetic shifts
INST(asl64RR) { arithmeticRR<u64>(opPtr, regPtr, ALSH); }
INST(asl64RV) { arithmeticRV<u64, u8>(opPtr, regPtr, ALSH); }
INST(asl64RM) { arithmeticRM<u64>(memory, opPtr, regPtr, ALSH); }
INST(asr64RR) { arithmeticRR<u64>(opPtr, regPtr, ARSH); }
INST(asr64RV) { arithmeticRV<u64, u8>(opPtr, regPtr, ARSH); }
INST(asr64RM) { arithmeticRM<u64>(memory, opPtr, regPtr, ARSH); }

/// ## 32 bit arithmetic shifts
INST(asl32RR) { arithmeticRR<u32>(opPtr, regPtr, ALSH); }
INST(asl32RV) { arithmeticRV<u32, u8>(opPtr, regPtr, ALSH); }
INST(asl32RM) { arithmeticRM<u32>(memory, opPtr, regPtr, ALSH); }
INST(asr32RR) { arithmeticRR<u32>(opPtr, regPtr, ARSH); }
INST(asr32RV) { arithmeticRV<u32, u8>(opPtr, regPtr, ARSH); }
INST(asr32RM) { arithmeticRM<u32>(memory, opPtr, regPtr, ARSH); }

/// ## 64 bit bitwise operations
INST(and64RR) { arithmeticRR<u64>(opPtr, regPtr, BitAnd); }
INST(and64RV) { arithmeticRV<u64>(opPtr, regPtr, BitAnd); }
INST(and64RM) { arithmeticRM<u64>(memory, opPtr, regPtr, BitAnd); }
INST(or64RR) { arithmeticRR<u64>(opPtr, regPtr, BitOr); }
INST(or64RV) { arithmeticRV<u64>(opPtr, regPtr, BitOr); }
INST(or64RM) { arithmeticRM<u64>(memory, opPtr, regPtr, BitOr); }
INST(xor64RR) { arithmeticRR<u64>(opPtr, regPtr, BitXOr); }
INST(xor64RV) { arithmeticRV<u64>(opPtr, regPtr, BitXOr); }
INST(xor64RM) { arithmeticRM<u64>(memory, opPtr, regPtr, BitXOr); }

/// ## 32 bit bitwise operations
INST(and32RR) { arithmeticRR<u32>(opPtr, regPtr, BitAnd); }
INST(and32RV) { arithmeticRV<u32>(opPtr, regPtr, BitAnd); }
INST(and32RM) { arithmeticRM<u32>(memory, opPtr, regPtr, BitAnd); }
INST(or32RR) { arithmeticRR<u32>(opPtr, regPtr, BitOr); }
INST(or32RV) { arithmeticRV<u32>(opPtr, regPtr, BitOr); }
INST(or32RM) { arithmeticRM<u32>(memory, opPtr, regPtr, BitOr); }
INST(xor32RR) { arithmeticRR<u32>(opPtr, regPtr, BitXOr); }
INST(xor32RV) { arithmeticRV<u32>(opPtr, regPtr, BitXOr); }
INST(xor32RM) { arithmeticRM<u32>(memory, opPtr, regPtr, BitXOr); }

/// ## Conversion
INST(sext1) { ::sext1(opPtr, regPtr); }
INST(sext8) { convert<i8, i64>(opPtr, regPtr); }
INST(sext16) { convert<i16, i64>(opPtr, regPtr); }
INST(sext32) { convert<i32, i64>(opPtr, regPtr); }
INST(fext) { convert<f32, f64>(opPtr, regPtr); }
INST(ftrunc) { convert<f64, f32>(opPtr, regPtr); }

INST(s8tof32) { convert<i8, f32>(opPtr, regPtr); }
INST(s16tof32) { convert<i16, f32>(opPtr, regPtr); }
INST(s32tof32) { convert<i32, f32>(opPtr, regPtr); }
INST(s64tof32) { convert<i64, f32>(opPtr, regPtr); }
INST(u8tof32) { convert<u8, f32>(opPtr, regPtr); }
INST(u16tof32) { convert<u16, f32>(opPtr, regPtr); }
INST(u32tof32) { convert<u32, f32>(opPtr, regPtr); }
INST(u64tof32) { convert<u64, f32>(opPtr, regPtr); }
INST(s8tof64) { convert<i8, f64>(opPtr, regPtr); }
INST(s16tof64) { convert<i16, f64>(opPtr, regPtr); }
INST(s32tof64) { convert<i32, f64>(opPtr, regPtr); }
INST(s64tof64) { convert<i64, f64>(opPtr, regPtr); }
INST(u8tof64) { convert<u8, f64>(opPtr, regPtr); }
INST(u16tof64) { convert<u16, f64>(opPtr, regPtr); }
INST(u32tof64) { convert<u32, f64>(opPtr, regPtr); }
INST(u64tof64) { convert<u64, f64>(opPtr, regPtr); }

INST(f32tos8) { convert<f32, i8>(opPtr, regPtr); }
INST(f32tos16) { convert<f32, i16>(opPtr, regPtr); }
INST(f32tos32) { convert<f32, i32>(opPtr, regPtr); }
INST(f32tos64) { convert<f32, i64>(opPtr, regPtr); }
INST(f32tou8) { convert<f32, u8>(opPtr, regPtr); }
INST(f32tou16) { convert<f32, u16>(opPtr, regPtr); }
INST(f32tou32) { convert<f32, u32>(opPtr, regPtr); }
INST(f32tou64) { convert<f32, u64>(opPtr, regPtr); }
INST(f64tos8) { convert<f64, i8>(opPtr, regPtr); }
INST(f64tos16) { convert<f64, i16>(opPtr, regPtr); }
INST(f64tos32) { convert<f64, i32>(opPtr, regPtr); }
INST(f64tos64) { convert<f64, i64>(opPtr, regPtr); }
INST(f64tou8) { convert<f64, u8>(opPtr, regPtr); }
INST(f64tou16) { convert<f64, u16>(opPtr, regPtr); }
INST(f64tou32) { convert<f64, u32>(opPtr, regPtr); }
INST(f64tou64) { convert<f64, u64>(opPtr, regPtr); }

#undef INST
#undef TERMINATE_EXECUTION
