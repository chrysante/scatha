// clang-format off

// ===----------------------------------------------------------------------===
// === List of all VM instructions -----------------------------------------===
// ===----------------------------------------------------------------------===

#ifndef SVM_INSTRUCTION_DEF
#   define SVM_INSTRUCTION_DEF(inst, class)
#endif

/// ## Function call and return
/// Performs the following operations:
///
///     regPtr    += regOffset
///     regPtr[-3] = stackPtr
///     regPtr[-2] = regOffset
///     regPtr[-1] = iptr
///     jmp dest
///
SVM_INSTRUCTION_DEF(call, Other) // (u32 dest, u8 regOffset)

/// Indirect call from register
/// Performs the same operations as call except that the `dest` argument is
/// not hardcoded but loaded from the register `destRegIdx`
SVM_INSTRUCTION_DEF(icallr, Other) // (u8 destRegIdx, u8 regOffset)

/// Indirect call from memory
/// Performs the same operations as call except that the `dest` argument is
/// not hardcoded but loaded from the memory pointer `dest`
SVM_INSTRUCTION_DEF(icallm, Other) // (MEMORY_POINTER dest, u8 regOffset)

/// Return to caller. Effectively performs the following operations:
///
///     iptr     = regPtr[-1]
///     stackPtr = regPtr[-3]
///     regPtr  -= regPtr[-2]
///
SVM_INSTRUCTION_DEF(ret, Other)

/// Invokes the foreign function at `index`
///
///     foreignFunctionTable[index](regPtr + regPtrOffset)
///
SVM_INSTRUCTION_DEF(cfng, Other) // (u8 regPtrOffset, u16 index)

/// Invokes the builtin function at `index`
///
///     builtinFunctionTable[index](regPtr + regPtrOffset)
///
SVM_INSTRUCTION_DEF(cbltn, Other) // (u8 regPtrOffset, u16 index)

/// Immediately terminates the program.
SVM_INSTRUCTION_DEF(terminate, Other)

/// ## Loads and stores
/// Copies the value from the source operand (right) into the destination
/// operand (right).
SVM_INSTRUCTION_DEF(mov64RR, RR)   // (u8 destRegIdx,  u8 sourceRegIdx)
SVM_INSTRUCTION_DEF(mov64RV, RV64) // (u8 destRegIdx,  u64 sourceValue)
SVM_INSTRUCTION_DEF(mov8MR,  MR)   // (MEMORY_POINTER, u8 sourceRegIdx)
SVM_INSTRUCTION_DEF(mov16MR, MR)   // (MEMORY_POINTER, u8 sourceRegIdx)
SVM_INSTRUCTION_DEF(mov32MR, MR)   // (MEMORY_POINTER, u8 sourceRegIdx)
SVM_INSTRUCTION_DEF(mov64MR, MR)   // (MEMORY_POINTER, u8 sourceRegIdx)
SVM_INSTRUCTION_DEF(mov8RM,  RM)   // (u8 destRegIdx,  MEMORY_POINTER)
SVM_INSTRUCTION_DEF(mov16RM, RM)   // (u8 destRegIdx,  MEMORY_POINTER)
SVM_INSTRUCTION_DEF(mov32RM, RM)   // (u8 destRegIdx,  MEMORY_POINTER)
SVM_INSTRUCTION_DEF(mov64RM, RM)   // (u8 destRegIdx,  MEMORY_POINTER)

/// ## Conditional moves
/// `cmov` instruction cannot move _to_ memory, only to registers.
/// Perform move if `equal` flag is set
SVM_INSTRUCTION_DEF(cmove64RR, RR)   // (u8 destRegIdx, u8 sourceRegIdx)
SVM_INSTRUCTION_DEF(cmove64RV, RV64) // (u8 destRegIdx, u64 sourceValue)
SVM_INSTRUCTION_DEF(cmove8RM,  RM)   // (u8 destRegIdx, MEMORY_POINTER)
SVM_INSTRUCTION_DEF(cmove16RM, RM)   // (u8 destRegIdx, MEMORY_POINTER)
SVM_INSTRUCTION_DEF(cmove32RM, RM)   // (u8 destRegIdx, MEMORY_POINTER)
SVM_INSTRUCTION_DEF(cmove64RM, RM)   // (u8 destRegIdx, MEMORY_POINTER)

/// Perform move if `not equal` flag is set
SVM_INSTRUCTION_DEF(cmovne64RR, RR)   // (u8 destRegIdx, u8 sourceRegIdx)
SVM_INSTRUCTION_DEF(cmovne64RV, RV64) // (u8 destRegIdx, u64 sourceValue)
SVM_INSTRUCTION_DEF(cmovne8RM,  RM)   // (u8 destRegIdx, MEMORY_POINTER)
SVM_INSTRUCTION_DEF(cmovne16RM, RM)   // (u8 destRegIdx, MEMORY_POINTER)
SVM_INSTRUCTION_DEF(cmovne32RM, RM)   // (u8 destRegIdx, MEMORY_POINTER)
SVM_INSTRUCTION_DEF(cmovne64RM, RM)   // (u8 destRegIdx, MEMORY_POINTER)

/// Perform move if `less` flag is set
SVM_INSTRUCTION_DEF(cmovl64RR, RR)   // (u8 destRegIdx, u8 sourceRegIdx)
SVM_INSTRUCTION_DEF(cmovl64RV, RV64) // (u8 destRegIdx, u64 sourceValue)
SVM_INSTRUCTION_DEF(cmovl8RM,  RM)   // (u8 destRegIdx, MEMORY_POINTER)
SVM_INSTRUCTION_DEF(cmovl16RM, RM)   // (u8 destRegIdx, MEMORY_POINTER)
SVM_INSTRUCTION_DEF(cmovl32RM, RM)   // (u8 destRegIdx, MEMORY_POINTER)
SVM_INSTRUCTION_DEF(cmovl64RM, RM)   // (u8 destRegIdx, MEMORY_POINTER)

/// Perform move if `less or equal` flag is set
SVM_INSTRUCTION_DEF(cmovle64RR, RR)   // (u8 destRegIdx, u8 sourceRegIdx)
SVM_INSTRUCTION_DEF(cmovle64RV, RV64) // (u8 destRegIdx, u64 sourceValue)
SVM_INSTRUCTION_DEF(cmovle8RM,  RM)   // (u8 destRegIdx, MEMORY_POINTER)
SVM_INSTRUCTION_DEF(cmovle16RM, RM)   // (u8 destRegIdx, MEMORY_POINTER)
SVM_INSTRUCTION_DEF(cmovle32RM, RM)   // (u8 destRegIdx, MEMORY_POINTER)
SVM_INSTRUCTION_DEF(cmovle64RM, RM)   // (u8 destRegIdx, MEMORY_POINTER)

/// Perform move if `greater` flag is set
SVM_INSTRUCTION_DEF(cmovg64RR, RR)   // (u8 destRegIdx, u8 sourceRegIdx)
SVM_INSTRUCTION_DEF(cmovg64RV, RV64) // (u8 destRegIdx, u64 sourceValue)
SVM_INSTRUCTION_DEF(cmovg8RM,  RM)   // (u8 destRegIdx, MEMORY_POINTER)
SVM_INSTRUCTION_DEF(cmovg16RM, RM)   // (u8 destRegIdx, MEMORY_POINTER)
SVM_INSTRUCTION_DEF(cmovg32RM, RM)   // (u8 destRegIdx, MEMORY_POINTER)
SVM_INSTRUCTION_DEF(cmovg64RM, RM)   // (u8 destRegIdx, MEMORY_POINTER)

/// Perform move if `greater or equal` flag is set
SVM_INSTRUCTION_DEF(cmovge64RR, RR)   // (u8 destRegIdx, u8 sourceRegIdx)
SVM_INSTRUCTION_DEF(cmovge64RV, RV64) // (u8 destRegIdx, u64 sourceValue)
SVM_INSTRUCTION_DEF(cmovge8RM,  RM)   // (u8 destRegIdx, MEMORY_POINTER)
SVM_INSTRUCTION_DEF(cmovge16RM, RM)   // (u8 destRegIdx, MEMORY_POINTER)
SVM_INSTRUCTION_DEF(cmovge32RM, RM)   // (u8 destRegIdx, MEMORY_POINTER)
SVM_INSTRUCTION_DEF(cmovge64RM, RM)   // (u8 destRegIdx, MEMORY_POINTER)

/// Store the stack pointer into register `destRegIdx` and increment it  by
/// `offset` bytes
/// \Warning: `offset` must be a multiple of 8 bytes.
SVM_INSTRUCTION_DEF(lincsp, Other)  // (u8 destRegIdx, u16 offset)

/// Load effective address. Compute the address specified by `MEMORY_POINTER`
/// and store it in register `destRegIdx`
SVM_INSTRUCTION_DEF(lea, RM) // (u8 destRegIdx, MEMORY_POINTER)

/// ## Jumps
/// Jumps are performed by assigning the `dest` argument to the instruction
/// pointer.

/// Jump to the specified offset.
SVM_INSTRUCTION_DEF(jmp, Jump) // (u32 dest)

/// Jump to the specified offset if equal flag is set.
SVM_INSTRUCTION_DEF(je,  Jump) // (u32 dest)

/// Jump to the specified offset if equal flag is not set.
SVM_INSTRUCTION_DEF(jne, Jump) // (u32 dest)

/// Jump to the specified offset if less flag is set.
SVM_INSTRUCTION_DEF(jl,  Jump) // (u32 dest)

/// Jump to the specified offset if less flag or equal flag is set.
SVM_INSTRUCTION_DEF(jle, Jump) // (u32 dest)

/// Jump to the specified offset if less flag and equal flag are not set.
SVM_INSTRUCTION_DEF(jg,  Jump) // (u32 dest)

/// Jump to the specified offset if less flag is not set.
SVM_INSTRUCTION_DEF(jge, Jump) // (u32 dest)

/// ## Comparison
/// Compare the operands and set flags accordingly
SVM_INSTRUCTION_DEF(ucmp8RR,  RR) // (u8 regIdxA, u8 regIdxB)
SVM_INSTRUCTION_DEF(ucmp16RR, RR) // (u8 regIdxA, u8 regIdxB)
SVM_INSTRUCTION_DEF(ucmp32RR, RR) // (u8 regIdxA, u8 regIdxB)
SVM_INSTRUCTION_DEF(ucmp64RR, RR) // (u8 regIdxA, u8 regIdxB)

SVM_INSTRUCTION_DEF(scmp8RR,  RR) // (u8 regIdxA, u8 regIdxB)
SVM_INSTRUCTION_DEF(scmp16RR, RR) // (u8 regIdxA, u8 regIdxB)
SVM_INSTRUCTION_DEF(scmp32RR, RR) // (u8 regIdxA, u8 regIdxB)
SVM_INSTRUCTION_DEF(scmp64RR, RR) // (u8 regIdxA, u8 regIdxB)

SVM_INSTRUCTION_DEF(ucmp8RV,  RV64) // (u8 regIdxA, u64 value)
SVM_INSTRUCTION_DEF(ucmp16RV, RV64) // (u8 regIdxA, u64 value)
SVM_INSTRUCTION_DEF(ucmp32RV, RV64) // (u8 regIdxA, u64 value)
SVM_INSTRUCTION_DEF(ucmp64RV, RV64) // (u8 regIdxA, u64 value)

SVM_INSTRUCTION_DEF(scmp8RV,  RV64) // (u8 regIdxA, u64 value)
SVM_INSTRUCTION_DEF(scmp16RV, RV64) // (u8 regIdxA, u64 value)
SVM_INSTRUCTION_DEF(scmp32RV, RV64) // (u8 regIdxA, u64 value)
SVM_INSTRUCTION_DEF(scmp64RV, RV64) // (u8 regIdxA, u64 value)

SVM_INSTRUCTION_DEF(fcmp32RR, RR) // (u8 regIdxA, u8 regIdxB)
SVM_INSTRUCTION_DEF(fcmp64RR, RR) // (u8 regIdxA, u8 regIdxB)

SVM_INSTRUCTION_DEF(fcmp32RV, RV64) // (u8 regIdxA, u64 value)
SVM_INSTRUCTION_DEF(fcmp64RV, RV64) // (u8 regIdxA, u64 value)

/// Compare the operand to 0 and set flags accordingly
SVM_INSTRUCTION_DEF(stest8,  R) //  (u8 regIdx)
SVM_INSTRUCTION_DEF(stest16, R) //  (u8 regIdx)
SVM_INSTRUCTION_DEF(stest32, R) //  (u8 regIdx)
SVM_INSTRUCTION_DEF(stest64, R) //  (u8 regIdx)

SVM_INSTRUCTION_DEF(utest8,  R) //  (u8 regIdx)
SVM_INSTRUCTION_DEF(utest16, R) //  (u8 regIdx)
SVM_INSTRUCTION_DEF(utest32, R) //  (u8 regIdx)
SVM_INSTRUCTION_DEF(utest64, R) //  (u8 regIdx)

/// ## Read comparison results
/// Set register to 0 or 1 based of the flags set by the `*cmp*` or `*test`
/// instructions
SVM_INSTRUCTION_DEF(sete,  R) // (u8 regIdx)
SVM_INSTRUCTION_DEF(setne, R) // (u8 regIdx)
SVM_INSTRUCTION_DEF(setl,  R) // (u8 regIdx)
SVM_INSTRUCTION_DEF(setle, R) // (u8 regIdx)
SVM_INSTRUCTION_DEF(setg,  R) // (u8 regIdx)
SVM_INSTRUCTION_DEF(setge, R) // (u8 regIdx)

/// ## Unary operations
/// reg[regIdx] = !reg[regIdx]
SVM_INSTRUCTION_DEF(lnt, R) // (u8 regIdx)
/// reg[regIdx] = ~reg[regIdx]
SVM_INSTRUCTION_DEF(bnt, R) // (u8 regIdx)
/// reg[regIdx] = -reg[regIdx]
SVM_INSTRUCTION_DEF(neg8,  R) // (u8 regIdx)
SVM_INSTRUCTION_DEF(neg16, R) // (u8 regIdx)
SVM_INSTRUCTION_DEF(neg32, R) // (u8 regIdx)
SVM_INSTRUCTION_DEF(neg64, R) // (u8 regIdx)

/// ## 64 bit integer arithmetic

/// reg[regIdxA] += reg[regIdxA]
SVM_INSTRUCTION_DEF(add64RR, RR) // (u8 regIdxA, u8 regIdxA)

/// reg[regIdx] += value
SVM_INSTRUCTION_DEF(add64RV, RV64) // (u8 regIdx, u64 value)

/// reg[regIdxA] += memory[eval(MEMORY_POINTER)]
SVM_INSTRUCTION_DEF(add64RM, RM) // (u8 regIdxA, MEMORY_POINTER)

/// reg[regIdxA] -= reg[regIdxA]
SVM_INSTRUCTION_DEF(sub64RR, RR) // (u8 regIdxA, u8 regIdxA)

/// reg[regIdx] -= value
SVM_INSTRUCTION_DEF(sub64RV, RV64) // (u8 regIdx, u64 value)

/// reg[regIdxA] -= memory[eval(MEMORY_POINTER)]
SVM_INSTRUCTION_DEF(sub64RM, RM) // (u8 regIdxA, MEMORY_POINTER)

/// reg[regIdxA] *= reg[regIdxA]
SVM_INSTRUCTION_DEF(mul64RR, RR) // (u8 regIdxA, u8 regIdxA)

/// reg[regIdx] *= value
SVM_INSTRUCTION_DEF(mul64RV, RV64) // (u8 regIdx, u64 value)

/// reg[regIdxA] *= memory[eval(MEMORY_POINTER)]
SVM_INSTRUCTION_DEF(mul64RM, RM) // (u8 regIdxA, MEMORY_POINTER)

/// reg[regIdxA] /= reg[regIdxA]
SVM_INSTRUCTION_DEF(udiv64RR, RR) // (u8 regIdxA, u8 regIdxA)

/// reg[regIdx] /= value
SVM_INSTRUCTION_DEF(udiv64RV, RV64) // (u8 regIdx, u64 value)

/// reg[regIdxA] /= memory[eval(MEMORY_POINTER)]
SVM_INSTRUCTION_DEF(udiv64RM, RM) // (u8 regIdxA, MEMORY_POINTER)

/// reg[regIdxA] /= reg[regIdxA]
SVM_INSTRUCTION_DEF(sdiv64RR, RR) // (u8 regIdxA, u8 regIdxA)

/// reg[regIdx] /= value (signed arithmetic)
SVM_INSTRUCTION_DEF(sdiv64RV, RV64) // (u8 regIdx, u64 value) (signed arithmetic)

/// reg[regIdxA] /= memory[eval(MEMORY_POINTER)]
SVM_INSTRUCTION_DEF(sdiv64RM, RM) // (u8 regIdxA, MEMORY_POINTER) (signed arithmetic)

/// reg[regIdxA] %= reg[regIdxA]
SVM_INSTRUCTION_DEF(urem64RR,  RR) // (u8 regIdxA, u8 regIdxA)

/// reg[regIdx] %= value
SVM_INSTRUCTION_DEF(urem64RV,  RV64) // (u8 regIdx, u64 value)

/// reg[regIdxA] %= memory[eval(MEMORY_POINTER)]
SVM_INSTRUCTION_DEF(urem64RM,  RM) // (u8 regIdxA, MEMORY_POINTER)

/// reg[regIdxA] %= reg[regIdxA]
SVM_INSTRUCTION_DEF(srem64RR, RR) // (u8 regIdxA, u8 regIdxA) (signed arithmetic)

/// reg[regIdx] %= value
SVM_INSTRUCTION_DEF(srem64RV, RV64) // (u8 regIdx, u64 value) (signed arithmetic)

/// reg[regIdxA] %= memory[eval(MEMORY_POINTER)]
SVM_INSTRUCTION_DEF(srem64RM, RM) // (u8 regIdxA, MEMORY_POINTER) (signed arithmetic)

/// ## 32 bit integer arithmetic

/// reg[regIdxA] += reg[regIdxA]
SVM_INSTRUCTION_DEF(add32RR, RR) // (u8 regIdxA, u8 regIdxA)

/// reg[regIdx] += value
SVM_INSTRUCTION_DEF(add32RV, RV32) // (u8 regIdx, u32 value)

/// reg[regIdxA] += memory[eval(MEMORY_POINTER)]
SVM_INSTRUCTION_DEF(add32RM, RM) // (u8 regIdxA, MEMORY_POINTER)

/// reg[regIdxA] -= reg[regIdxA]
SVM_INSTRUCTION_DEF(sub32RR, RR) // (u8 regIdxA, u8 regIdxA)

/// reg[regIdx] -= value
SVM_INSTRUCTION_DEF(sub32RV, RV32) // (u8 regIdx, u32 value)

/// reg[regIdxA] -= memory[eval(MEMORY_POINTER)]
SVM_INSTRUCTION_DEF(sub32RM, RM) // (u8 regIdxA, MEMORY_POINTER)

/// reg[regIdxA] *= reg[regIdxA]
SVM_INSTRUCTION_DEF(mul32RR, RR) // (u8 regIdxA, u8 regIdxA)

/// reg[regIdx] *= value
SVM_INSTRUCTION_DEF(mul32RV, RV32) // (u8 regIdx, u32 value)

/// reg[regIdxA] *= memory[eval(MEMORY_POINTER)]
SVM_INSTRUCTION_DEF(mul32RM, RM) // (u8 regIdxA, MEMORY_POINTER)

/// reg[regIdxA] /= reg[regIdxA]
SVM_INSTRUCTION_DEF(udiv32RR, RR) // (u8 regIdxA, u8 regIdxA)

/// reg[regIdx] /= value
SVM_INSTRUCTION_DEF(udiv32RV, RV32) // (u8 regIdx, u32 value)

/// reg[regIdxA] /= memory[eval(MEMORY_POINTER)]
SVM_INSTRUCTION_DEF(udiv32RM, RM) // (u8 regIdxA, MEMORY_POINTER)

/// reg[regIdxA] /= reg[regIdxA]
SVM_INSTRUCTION_DEF(sdiv32RR, RR) // (u8 regIdxA, u8 regIdxA)

/// reg[regIdx] /= value (signed arithmetic)
SVM_INSTRUCTION_DEF(sdiv32RV, RV32) // (u8 regIdx, u32 value) (signed arithmetic)

/// reg[regIdxA] /= memory[eval(MEMORY_POINTER)]
SVM_INSTRUCTION_DEF(sdiv32RM, RM) // (u8 regIdxA, MEMORY_POINTER) (signed arithmetic)

/// reg[regIdxA] %= reg[regIdxA]
SVM_INSTRUCTION_DEF(urem32RR,  RR) // (u8 regIdxA, u8 regIdxA)

/// reg[regIdx] %= value
SVM_INSTRUCTION_DEF(urem32RV,  RV32) // (u8 regIdx, u32 value)

/// reg[regIdxA] %= memory[eval(MEMORY_POINTER)]
SVM_INSTRUCTION_DEF(urem32RM,  RM) // (u8 regIdxA, MEMORY_POINTER)

/// reg[regIdxA] %= reg[regIdxA]
SVM_INSTRUCTION_DEF(srem32RR, RR) // (u8 regIdxA, u8 regIdxA) (signed arithmetic)

/// reg[regIdx] %= value
SVM_INSTRUCTION_DEF(srem32RV, RV32) // (u8 regIdx, u32 value) (signed arithmetic)

/// reg[regIdxA] %= memory[eval(MEMORY_POINTER)]
SVM_INSTRUCTION_DEF(srem32RM, RM) // (u8 regIdxA, MEMORY_POINTER) (signed arithmetic)

/// ## 64 bit floating point arithmetic

/// reg[regIdxA] += reg[regIdxA]
SVM_INSTRUCTION_DEF(fadd64RR, RR) // (u8 regIdxA, u8 regIdxA)

/// reg[regIdx] += value
SVM_INSTRUCTION_DEF(fadd64RV, RV64) // (u8 regIdx, u64 value)

/// reg[regIdxA] += memory[eval(MEMORY_POINTER)]
SVM_INSTRUCTION_DEF(fadd64RM, RM) // (u8 regIdxA, MEMORY_POINTER)

/// reg[regIdxA] -= reg[regIdxA]
SVM_INSTRUCTION_DEF(fsub64RR, RR) // (u8 regIdxA, u8 regIdxA)

/// reg[regIdx] -= value
SVM_INSTRUCTION_DEF(fsub64RV, RV64) // (u8 regIdx, u64 value)

/// reg[regIdxA] -= memory[eval(MEMORY_POINTER)]
SVM_INSTRUCTION_DEF(fsub64RM, RM) // (u8 regIdxA, MEMORY_POINTER)

/// reg[regIdxA] *= reg[regIdxA]
SVM_INSTRUCTION_DEF(fmul64RR, RR) // (u8 regIdxA, u8 regIdxA)

/// reg[regIdx] *= value
SVM_INSTRUCTION_DEF(fmul64RV, RV64) // (u8 regIdx, u64 value)

/// reg[regIdxA] *= memory[eval(MEMORY_POINTER)]
SVM_INSTRUCTION_DEF(fmul64RM, RM) // (u8 regIdxA, MEMORY_POINTER)

/// reg[regIdxA] /= reg[regIdxA]
SVM_INSTRUCTION_DEF(fdiv64RR, RR) // (u8 regIdxA, u8 regIdxA)

/// reg[regIdx]  /= value
SVM_INSTRUCTION_DEF(fdiv64RV, RV64) // (u8 regIdx, u64 value)

/// reg[regIdxA] /= memory[eval(MEMORY_POINTER)]
SVM_INSTRUCTION_DEF(fdiv64RM, RM) // (u8 regIdxA, MEMORY_POINTER)

/// ## 32 bit floating point arithmetic

/// reg[regIdxA] += reg[regIdxA]
SVM_INSTRUCTION_DEF(fadd32RR, RR) // (u8 regIdxA, u8 regIdxA)

/// reg[regIdx] += value
SVM_INSTRUCTION_DEF(fadd32RV, RV32) // (u8 regIdx, u32 value)

/// reg[regIdxA] += memory[eval(MEMORY_POINTER)]
SVM_INSTRUCTION_DEF(fadd32RM, RM) // (u8 regIdxA, MEMORY_POINTER)

/// reg[regIdxA] -= reg[regIdxA]
SVM_INSTRUCTION_DEF(fsub32RR, RR) // (u8 regIdxA, u8 regIdxA)

/// reg[regIdx] -= value
SVM_INSTRUCTION_DEF(fsub32RV, RV32) // (u8 regIdx, u32 value)

/// reg[regIdxA] -= memory[eval(MEMORY_POINTER)]
SVM_INSTRUCTION_DEF(fsub32RM, RM) // (u8 regIdxA, MEMORY_POINTER)

/// reg[regIdxA] *= reg[regIdxA]
SVM_INSTRUCTION_DEF(fmul32RR, RR) // (u8 regIdxA, u8 regIdxA)

/// reg[regIdx] *= value
SVM_INSTRUCTION_DEF(fmul32RV, RV32) // (u8 regIdx, u32 value)

/// reg[regIdxA] *= memory[eval(MEMORY_POINTER)]
SVM_INSTRUCTION_DEF(fmul32RM, RM) // (u8 regIdxA, MEMORY_POINTER)

/// reg[regIdxA] /= reg[regIdxA]
SVM_INSTRUCTION_DEF(fdiv32RR, RR) // (u8 regIdxA, u8 regIdxA)

/// reg[regIdx]  /= value
SVM_INSTRUCTION_DEF(fdiv32RV, RV32) // (u8 regIdx, u32 value)

/// reg[regIdxA] /= memory[eval(MEMORY_POINTER)]
SVM_INSTRUCTION_DEF(fdiv32RM, RM) // (u8 regIdxA, MEMORY_POINTER)

/// ## Logical bitshift
SVM_INSTRUCTION_DEF(lsl64RR, RR)   //  (u8 regIdxA, u8 regIdxB)
SVM_INSTRUCTION_DEF(lsl64RV, RV8)  //  (u8 regIdxA, u8 value)
SVM_INSTRUCTION_DEF(lsl64RM, RM)   //  (u8 regIdxA, MEMORY_POINTER)
SVM_INSTRUCTION_DEF(lsr64RR, RR)   //  (u8 regIdxA, u8 regIdxB)
SVM_INSTRUCTION_DEF(lsr64RV, RV8)  //  (u8 regIdxA, u8 value)
SVM_INSTRUCTION_DEF(lsr64RM, RM)   //  (u8 regIdxA, MEMORY_POINTER)

SVM_INSTRUCTION_DEF(lsl32RR, RR)   //  (u8 regIdxA, u8 regIdxB)
SVM_INSTRUCTION_DEF(lsl32RV, RV8)  //  (u8 regIdxA, u8 value)
SVM_INSTRUCTION_DEF(lsl32RM, RM)   //  (u8 regIdxA, MEMORY_POINTER)
SVM_INSTRUCTION_DEF(lsr32RR, RR)   //  (u8 regIdxA, u8 regIdxB)
SVM_INSTRUCTION_DEF(lsr32RV, RV8)  //  (u8 regIdxA, u8 value)
SVM_INSTRUCTION_DEF(lsr32RM, RM)   //  (u8 regIdxA, MEMORY_POINTER)

/// ## Arithmetic bitshift
SVM_INSTRUCTION_DEF(asl64RR, RR)   //  (u8 regIdxA, u8 regIdxB)
SVM_INSTRUCTION_DEF(asl64RV, RV8)  //  (u8 regIdxA, u8 value)
SVM_INSTRUCTION_DEF(asl64RM, RM)   //  (u8 regIdxA, MEMORY_POINTER)
SVM_INSTRUCTION_DEF(asr64RR, RR)   //  (u8 regIdxA, u8 regIdxB)
SVM_INSTRUCTION_DEF(asr64RV, RV8)  //  (u8 regIdxA, u8 value)
SVM_INSTRUCTION_DEF(asr64RM, RM)   //  (u8 regIdxA, MEMORY_POINTER)

SVM_INSTRUCTION_DEF(asl32RR, RR)   //  (u8 regIdxA, u8 regIdxB)
SVM_INSTRUCTION_DEF(asl32RV, RV8)  //  (u8 regIdxA, u8 value)
SVM_INSTRUCTION_DEF(asl32RM, RM)   //  (u8 regIdxA, MEMORY_POINTER)
SVM_INSTRUCTION_DEF(asr32RR, RR)   //  (u8 regIdxA, u8 regIdxB)
SVM_INSTRUCTION_DEF(asr32RV, RV8)  //  (u8 regIdxA, u8 value)
SVM_INSTRUCTION_DEF(asr32RM, RM)   //  (u8 regIdxA, MEMORY_POINTER)

/// ## Bitwise AND/OR
SVM_INSTRUCTION_DEF(and64RR, RR)   //  (u8 regIdxA, u8 regIdxB)
SVM_INSTRUCTION_DEF(and64RV, RV64) //  (u8 regIdxA, u64 value)
SVM_INSTRUCTION_DEF(and64RM, RM)   //  (u8 regIdxA, MEMORY_POINTER)
SVM_INSTRUCTION_DEF(or64RR,  RR)   //  (u8 regIdxA, u8 regIdxB)
SVM_INSTRUCTION_DEF(or64RV,  RV64) //  (u8 regIdxA, u64 value)
SVM_INSTRUCTION_DEF(or64RM,  RM)   //  (u8 regIdxA, MEMORY_POINTER)
SVM_INSTRUCTION_DEF(xor64RR, RR)   //  (u8 regIdxA, u8 regIdxB)
SVM_INSTRUCTION_DEF(xor64RV, RV64) //  (u8 regIdxA, u64 value)
SVM_INSTRUCTION_DEF(xor64RM, RM)   //  (u8 regIdxA, MEMORY_POINTER)

SVM_INSTRUCTION_DEF(and32RR, RR)   //  (u8 regIdxA, u8 regIdxB)
SVM_INSTRUCTION_DEF(and32RV, RV32) //  (u8 regIdxA, u32 value)
SVM_INSTRUCTION_DEF(and32RM, RM)   //  (u8 regIdxA, MEMORY_POINTER)
SVM_INSTRUCTION_DEF(or32RR,  RR)   //  (u8 regIdxA, u8 regIdxB)
SVM_INSTRUCTION_DEF(or32RV,  RV32) //  (u8 regIdxA, u32 value)
SVM_INSTRUCTION_DEF(or32RM,  RM)   //  (u8 regIdxA, MEMORY_POINTER)
SVM_INSTRUCTION_DEF(xor32RR, RR)   //  (u8 regIdxA, u8 regIdxB)
SVM_INSTRUCTION_DEF(xor32RV, RV32) //  (u8 regIdxA, u32 value)
SVM_INSTRUCTION_DEF(xor32RM, RM)   //  (u8 regIdxA, MEMORY_POINTER)

/// ## Conversion
/// Sign extend the value in register `regIdx`.
/// The value is either interpreted as 1, 8, 16, 32 or 64 bit and sign extended to
/// 64 bits wide.
SVM_INSTRUCTION_DEF(sext1,  R) //  (u8 regIdx)
SVM_INSTRUCTION_DEF(sext8,  R) //  (u8 regIdx)
SVM_INSTRUCTION_DEF(sext16, R) //  (u8 regIdx)
SVM_INSTRUCTION_DEF(sext32, R) //  (u8 regIdx)

/// Convert the floating point value in register `regIdx`.
/// The value is either extended from 32 bits to 64 bits or truncated from 64 bits to 32 bits.
SVM_INSTRUCTION_DEF(fext, R)   //  (u8 regIdx)
SVM_INSTRUCTION_DEF(ftrunc, R) //  (u8 regIdx)

/// Convert the integral value in register `regIdx` to a floating point value.
SVM_INSTRUCTION_DEF(s8tof32,   R) //  (u8 regIdx)
SVM_INSTRUCTION_DEF(s16tof32,  R) //  (u8 regIdx)
SVM_INSTRUCTION_DEF(s32tof32,  R) //  (u8 regIdx)
SVM_INSTRUCTION_DEF(s64tof32,  R) //  (u8 regIdx)
SVM_INSTRUCTION_DEF(u8tof32,   R) //  (u8 regIdx)
SVM_INSTRUCTION_DEF(u16tof32,  R) //  (u8 regIdx)
SVM_INSTRUCTION_DEF(u32tof32,  R) //  (u8 regIdx)
SVM_INSTRUCTION_DEF(u64tof32,  R) //  (u8 regIdx)
SVM_INSTRUCTION_DEF(s8tof64,   R) //  (u8 regIdx)
SVM_INSTRUCTION_DEF(s16tof64,  R) //  (u8 regIdx)
SVM_INSTRUCTION_DEF(s32tof64,  R) //  (u8 regIdx)
SVM_INSTRUCTION_DEF(s64tof64,  R) //  (u8 regIdx)
SVM_INSTRUCTION_DEF(u8tof64,   R) //  (u8 regIdx)
SVM_INSTRUCTION_DEF(u16tof64,  R) //  (u8 regIdx)
SVM_INSTRUCTION_DEF(u32tof64,  R) //  (u8 regIdx)
SVM_INSTRUCTION_DEF(u64tof64,  R) //  (u8 regIdx)

/// Convert the floating point value in register `regIdx` to an integral value.
SVM_INSTRUCTION_DEF(f32tos8,   R) //  (u8 regIdx)
SVM_INSTRUCTION_DEF(f32tos16,  R) //  (u8 regIdx)
SVM_INSTRUCTION_DEF(f32tos32,  R) //  (u8 regIdx)
SVM_INSTRUCTION_DEF(f32tos64,  R) //  (u8 regIdx)
SVM_INSTRUCTION_DEF(f32tou8,   R) //  (u8 regIdx)
SVM_INSTRUCTION_DEF(f32tou16,  R) //  (u8 regIdx)
SVM_INSTRUCTION_DEF(f32tou32,  R) //  (u8 regIdx)
SVM_INSTRUCTION_DEF(f32tou64,  R) //  (u8 regIdx)
SVM_INSTRUCTION_DEF(f64tos8,   R) //  (u8 regIdx)
SVM_INSTRUCTION_DEF(f64tos16,  R) //  (u8 regIdx)
SVM_INSTRUCTION_DEF(f64tos32,  R) //  (u8 regIdx)
SVM_INSTRUCTION_DEF(f64tos64,  R) //  (u8 regIdx)
SVM_INSTRUCTION_DEF(f64tou8,   R) //  (u8 regIdx)
SVM_INSTRUCTION_DEF(f64tou16,  R) //  (u8 regIdx)
SVM_INSTRUCTION_DEF(f64tou32,  R) //  (u8 regIdx)
SVM_INSTRUCTION_DEF(f64tou64,  R) //  (u8 regIdx)

#undef SVM_INSTRUCTION_DEF
