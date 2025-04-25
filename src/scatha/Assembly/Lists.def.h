// NO-INCLUDE-GUARD

// clang-format off

// ===----------------------------------------------------------------------===
// === Instructions --------------------------------------------------------===
// ===----------------------------------------------------------------------===

#ifndef SC_ASM_INSTRUCTION_DEF
#   define SC_ASM_INSTRUCTION_DEF(inst)
#endif

#ifndef SC_ASM_INSTRUCTION_SEPARATOR
#   define SC_ASM_INSTRUCTION_SEPARATOR
#endif

SC_ASM_INSTRUCTION_DEF(MoveInst)            SC_ASM_INSTRUCTION_SEPARATOR
SC_ASM_INSTRUCTION_DEF(CMoveInst)           SC_ASM_INSTRUCTION_SEPARATOR
SC_ASM_INSTRUCTION_DEF(JumpInst)            SC_ASM_INSTRUCTION_SEPARATOR
SC_ASM_INSTRUCTION_DEF(CallInst)            SC_ASM_INSTRUCTION_SEPARATOR
SC_ASM_INSTRUCTION_DEF(CallExtInst)         SC_ASM_INSTRUCTION_SEPARATOR
SC_ASM_INSTRUCTION_DEF(ReturnInst)          SC_ASM_INSTRUCTION_SEPARATOR
SC_ASM_INSTRUCTION_DEF(TerminateInst)       SC_ASM_INSTRUCTION_SEPARATOR
SC_ASM_INSTRUCTION_DEF(LIncSPInst)          SC_ASM_INSTRUCTION_SEPARATOR
SC_ASM_INSTRUCTION_DEF(LEAInst)             SC_ASM_INSTRUCTION_SEPARATOR
SC_ASM_INSTRUCTION_DEF(CompareInst)         SC_ASM_INSTRUCTION_SEPARATOR
SC_ASM_INSTRUCTION_DEF(TestInst)            SC_ASM_INSTRUCTION_SEPARATOR
SC_ASM_INSTRUCTION_DEF(SetInst)             SC_ASM_INSTRUCTION_SEPARATOR
SC_ASM_INSTRUCTION_DEF(UnaryArithmeticInst) SC_ASM_INSTRUCTION_SEPARATOR
SC_ASM_INSTRUCTION_DEF(ArithmeticInst)      SC_ASM_INSTRUCTION_SEPARATOR
SC_ASM_INSTRUCTION_DEF(TruncExtInst)        SC_ASM_INSTRUCTION_SEPARATOR
SC_ASM_INSTRUCTION_DEF(ConvertInst)

#undef SC_ASM_INSTRUCTION_DEF
#undef SC_ASM_INSTRUCTION_SEPARATOR

// ===----------------------------------------------------------------------===
// === Values --------------------------------------------------------------===
// ===----------------------------------------------------------------------===

#ifndef SC_ASM_VALUE_DEF
#   define SC_ASM_VALUE_DEF(inst)
#endif

#ifndef SC_ASM_VALUE_SEPARATOR
#   define SC_ASM_VALUE_SEPARATOR
#endif

SC_ASM_VALUE_DEF(RegisterIndex) SC_ASM_VALUE_SEPARATOR
SC_ASM_VALUE_DEF(MemoryAddress) SC_ASM_VALUE_SEPARATOR
SC_ASM_VALUE_DEF(Value8)        SC_ASM_VALUE_SEPARATOR
SC_ASM_VALUE_DEF(Value16)       SC_ASM_VALUE_SEPARATOR
SC_ASM_VALUE_DEF(Value32)       SC_ASM_VALUE_SEPARATOR
SC_ASM_VALUE_DEF(Value64)       SC_ASM_VALUE_SEPARATOR
SC_ASM_VALUE_DEF(LabelPosition)

#undef SC_ASM_VALUE_DEF
#undef SC_ASM_VALUE_SEPARATOR

// ===----------------------------------------------------------------------===
// === Compare -------------------------------------------------------------===
// ===----------------------------------------------------------------------===

#ifndef SC_ASM_COMPARE_DEF
#   define SC_ASM_COMPARE_DEF(elem, shortStr)
#endif

SC_ASM_COMPARE_DEF(None,      "")
SC_ASM_COMPARE_DEF(Less,      "l")
SC_ASM_COMPARE_DEF(LessEq,    "le")
SC_ASM_COMPARE_DEF(Greater,   "g")
SC_ASM_COMPARE_DEF(GreaterEq, "ge")
SC_ASM_COMPARE_DEF(Eq,        "e")
SC_ASM_COMPARE_DEF(NotEq,     "ne")

#undef SC_ASM_COMPARE_DEF

// ===----------------------------------------------------------------------===
// === Unary Arithmetic ----------------------------------------------------===
// ===----------------------------------------------------------------------===

#ifndef SC_ASM_UNARY_ARITHMETIC_DEF
#   define SC_ASM_UNARY_ARITHMETIC_DEF(op, instName)
#endif

SC_ASM_UNARY_ARITHMETIC_DEF(BitwiseNot, "bnt")
SC_ASM_UNARY_ARITHMETIC_DEF(LogicalNot, "lnt")
SC_ASM_UNARY_ARITHMETIC_DEF(Negate,     "neg")

#undef SC_ASM_UNARY_ARITHMETIC_DEF

// ===----------------------------------------------------------------------===
// === Arithmetic ----------------------------------------------------------===
// ===----------------------------------------------------------------------===

#ifndef SC_ASM_ARITHMETIC_DEF
#   define SC_ASM_ARITHMETIC_DEF(op, instName)
#endif

SC_ASM_ARITHMETIC_DEF(Add,  "add")
SC_ASM_ARITHMETIC_DEF(Sub,  "sub")
SC_ASM_ARITHMETIC_DEF(Mul,  "mul")
SC_ASM_ARITHMETIC_DEF(SDiv, "sdiv")
SC_ASM_ARITHMETIC_DEF(UDiv, "udiv")
SC_ASM_ARITHMETIC_DEF(SRem, "srem")
SC_ASM_ARITHMETIC_DEF(URem, "urem")
SC_ASM_ARITHMETIC_DEF(FAdd, "fadd")
SC_ASM_ARITHMETIC_DEF(FSub, "fsub")
SC_ASM_ARITHMETIC_DEF(FMul, "fmul")
SC_ASM_ARITHMETIC_DEF(FDiv, "fdiv")
SC_ASM_ARITHMETIC_DEF(LShL, "lshl")
SC_ASM_ARITHMETIC_DEF(LShR, "lshr")
SC_ASM_ARITHMETIC_DEF(AShL, "ashl")
SC_ASM_ARITHMETIC_DEF(AShR, "ashr")
SC_ASM_ARITHMETIC_DEF(And,  "and")
SC_ASM_ARITHMETIC_DEF(Or,   "or")
SC_ASM_ARITHMETIC_DEF(XOr,  "xor")

#undef SC_ASM_ARITHMETIC_DEF
