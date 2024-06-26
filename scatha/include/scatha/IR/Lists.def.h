// clang-format off

// ===----------------------------------------------------------------------===
// === List of all CFG Nodes -----------------------------------------------===
// ===----------------------------------------------------------------------===

#ifndef SC_VALUENODE_DEF
#   define SC_VALUENODE_DEF(Name, Parent, Corporeality)
#endif

SC_VALUENODE_DEF(Value,      VoidParent, Abstract)
SC_VALUENODE_DEF(Parameter,  Value,      Concrete)
SC_VALUENODE_DEF(BasicBlock, Value,      Concrete)
SC_VALUENODE_DEF(User,       Value,      Abstract)

/// Constants
SC_VALUENODE_DEF(Constant,              User,           Abstract)
SC_VALUENODE_DEF(IntegralConstant,      Constant,       Concrete)
SC_VALUENODE_DEF(FloatingPointConstant, Constant,       Concrete)
SC_VALUENODE_DEF(NullPointerConstant,   Constant,       Concrete)
SC_VALUENODE_DEF(RecordConstant,        Constant,       Abstract)
SC_VALUENODE_DEF(StructConstant,        RecordConstant, Concrete)
SC_VALUENODE_DEF(ArrayConstant,         RecordConstant, Concrete)
SC_VALUENODE_DEF(UndefValue,            Constant,       Concrete)

/// Globals
SC_VALUENODE_DEF(Global,          Constant, Abstract)
SC_VALUENODE_DEF(GlobalVariable,  Global,   Concrete)
SC_VALUENODE_DEF(Callable,        Global,   Abstract)
SC_VALUENODE_DEF(Function,        Callable, Concrete)
SC_VALUENODE_DEF(ForeignFunction, Callable, Concrete)

/// Instruction base classes
SC_VALUENODE_DEF(Instruction,       User,        Abstract)
SC_VALUENODE_DEF(UnaryInstruction,  Instruction, Abstract)
SC_VALUENODE_DEF(BinaryInstruction, Instruction, Abstract)
SC_VALUENODE_DEF(TerminatorInst,    Instruction, Abstract)
SC_VALUENODE_DEF(AccessValueInst,   Instruction, Abstract)

/// Instructions
#ifndef SC_INSTRUCTIONNODE_DEF
#   define SC_INSTRUCTIONNODE_DEF(Name, Parent, Corporeality)                  \
        SC_VALUENODE_DEF(Name, Parent, Corporeality)
#endif

SC_INSTRUCTIONNODE_DEF(Alloca,              Instruction,       Concrete)
SC_INSTRUCTIONNODE_DEF(Load,                Instruction,       Concrete)
SC_INSTRUCTIONNODE_DEF(Store,               Instruction,       Concrete)
SC_INSTRUCTIONNODE_DEF(ConversionInst,      UnaryInstruction,  Concrete)
SC_INSTRUCTIONNODE_DEF(CompareInst,         BinaryInstruction, Concrete)
SC_INSTRUCTIONNODE_DEF(UnaryArithmeticInst, UnaryInstruction,  Concrete)
SC_INSTRUCTIONNODE_DEF(ArithmeticInst,      BinaryInstruction, Concrete)
SC_INSTRUCTIONNODE_DEF(Goto,                TerminatorInst,    Concrete)
SC_INSTRUCTIONNODE_DEF(Branch,              TerminatorInst,    Concrete)
SC_INSTRUCTIONNODE_DEF(Return,              TerminatorInst,    Concrete)
SC_INSTRUCTIONNODE_DEF(Call,                Instruction,       Concrete)
SC_INSTRUCTIONNODE_DEF(Phi,                 Instruction,       Concrete)
SC_INSTRUCTIONNODE_DEF(Select,              Instruction,       Concrete)
SC_INSTRUCTIONNODE_DEF(GetElementPointer,   Instruction,       Concrete)
SC_INSTRUCTIONNODE_DEF(ExtractValue,        Instruction,       Concrete)
SC_INSTRUCTIONNODE_DEF(InsertValue,         Instruction,       Concrete) // Update LAST in Fwd.h if this changes

#undef SC_INSTRUCTIONNODE_DEF

#undef SC_VALUENODE_DEF

// ===----------------------------------------------------------------------===
// === List of all attributes  ---------------------------------------------===
// ===----------------------------------------------------------------------===

#ifndef SC_ATTRIBUTE_DEF
#   define SC_ATTRIBUTE_DEF(...)
#endif

SC_ATTRIBUTE_DEF(Attribute,       VoidParent,     Abstract)
SC_ATTRIBUTE_DEF(ByValAttribute,  Attribute,      Concrete)
SC_ATTRIBUTE_DEF(ValRetAttribute, Attribute,      Concrete) // Update LAST if this changes!

#undef SC_ATTRIBUTE_DEF


// ===----------------------------------------------------------------------===
// === List of conversions -------------------------------------------------===
// ===----------------------------------------------------------------------===

#ifndef SC_CONVERSION_DEF
#   define SC_CONVERSION_DEF(op, keyword)
#endif

/// Int conversions
SC_CONVERSION_DEF(Zext,    zext)
SC_CONVERSION_DEF(Sext,    sext)
SC_CONVERSION_DEF(Trunc,   trunc)

/// Float conversions
SC_CONVERSION_DEF(Fext,    fext)
SC_CONVERSION_DEF(Ftrunc,  ftrunc)

/// Int to float
SC_CONVERSION_DEF(UtoF,    utof)
SC_CONVERSION_DEF(StoF,    stof)

/// Float to int
SC_CONVERSION_DEF(FtoU,    ftou)
SC_CONVERSION_DEF(FtoS,    ftos)

/// Bitcast
SC_CONVERSION_DEF(Bitcast, bitcast)

#undef SC_CONVERSION_DEF

// ===----------------------------------------------------------------------===
// === List of compare modes -----------------------------------------------===
// ===----------------------------------------------------------------------===

#ifndef SC_COMPARE_MODE_DEF
#   define SC_COMPARE_MODE_DEF(op, keyword)
#endif

SC_COMPARE_MODE_DEF(Signed,   scmp)
SC_COMPARE_MODE_DEF(Unsigned, ucmp)
SC_COMPARE_MODE_DEF(Float,    fcmp)

#undef SC_COMPARE_MODE_DEF

// ===----------------------------------------------------------------------===
// === List of compare operations ------------------------------------------===
// ===----------------------------------------------------------------------===

#ifndef SC_COMPARE_OPERATION_DEF
#   define SC_COMPARE_OPERATION_DEF(op, keyword)
#endif

SC_COMPARE_OPERATION_DEF(Less,      ls)
SC_COMPARE_OPERATION_DEF(LessEq,    leq)
SC_COMPARE_OPERATION_DEF(Greater,   grt)
SC_COMPARE_OPERATION_DEF(GreaterEq, geq)
SC_COMPARE_OPERATION_DEF(Equal,     eq)
SC_COMPARE_OPERATION_DEF(NotEqual,  neq)

#undef SC_COMPARE_OPERATION_DEF

// ===----------------------------------------------------------------------===
// === List of unary arithmetic operations ---------------------------------===
// ===----------------------------------------------------------------------===

#ifndef SC_UNARY_ARITHMETIC_OPERATION_DEF
#   define SC_UNARY_ARITHMETIC_OPERATION_DEF(op, keyword)
#endif

SC_UNARY_ARITHMETIC_OPERATION_DEF(BitwiseNot, bnt)
SC_UNARY_ARITHMETIC_OPERATION_DEF(LogicalNot, lnt)
SC_UNARY_ARITHMETIC_OPERATION_DEF(Negate, neg)

#undef SC_UNARY_ARITHMETIC_OPERATION_DEF

// ===----------------------------------------------------------------------===
// === List of arithmetic operations ---------------------------------------===
// ===----------------------------------------------------------------------===

#ifndef SC_ARITHMETIC_OPERATION_DEF
#   define SC_ARITHMETIC_OPERATION_DEF(op, keyword)
#endif

SC_ARITHMETIC_OPERATION_DEF(Add,  add)
SC_ARITHMETIC_OPERATION_DEF(Sub,  sub)
SC_ARITHMETIC_OPERATION_DEF(Mul,  mul)
SC_ARITHMETIC_OPERATION_DEF(SDiv, sdiv)
SC_ARITHMETIC_OPERATION_DEF(UDiv, udiv)
SC_ARITHMETIC_OPERATION_DEF(SRem, srem)
SC_ARITHMETIC_OPERATION_DEF(URem, urem)
SC_ARITHMETIC_OPERATION_DEF(FAdd, fadd)
SC_ARITHMETIC_OPERATION_DEF(FSub, fsub)
SC_ARITHMETIC_OPERATION_DEF(FMul, fmul)
SC_ARITHMETIC_OPERATION_DEF(FDiv, fdiv)
SC_ARITHMETIC_OPERATION_DEF(LShL, lshl)
SC_ARITHMETIC_OPERATION_DEF(LShR, lshr)
SC_ARITHMETIC_OPERATION_DEF(AShL, ashl)
SC_ARITHMETIC_OPERATION_DEF(AShR, ashr)
SC_ARITHMETIC_OPERATION_DEF(And,  and)
SC_ARITHMETIC_OPERATION_DEF(Or,   or)
SC_ARITHMETIC_OPERATION_DEF(XOr,  xor)

#undef SC_ARITHMETIC_OPERATION_DEF

// ===----------------------------------------------------------------------===
// === List of all types categories ----------------------------------------===
// ===----------------------------------------------------------------------===

#ifndef SC_TYPE_CATEGORY_DEF
#   define SC_TYPE_CATEGORY_DEF(...)
#endif

SC_TYPE_CATEGORY_DEF(Type,           VoidParent,     Abstract)
SC_TYPE_CATEGORY_DEF(VoidType,       Type,           Concrete)
SC_TYPE_CATEGORY_DEF(PointerType,    Type,           Concrete)
SC_TYPE_CATEGORY_DEF(ArithmeticType, Type,           Abstract)
SC_TYPE_CATEGORY_DEF(IntegralType,   ArithmeticType, Concrete)
SC_TYPE_CATEGORY_DEF(FloatType,      ArithmeticType, Concrete)
SC_TYPE_CATEGORY_DEF(RecordType,     Type,           Abstract)
SC_TYPE_CATEGORY_DEF(StructType,     RecordType,     Concrete)
SC_TYPE_CATEGORY_DEF(ArrayType,      RecordType,     Concrete)
SC_TYPE_CATEGORY_DEF(FunctionType,   Type,           Concrete) // Update LAST if this changes!

#undef SC_TYPE_CATEGORY_DEF

// ===----------------------------------------------------------------------===
// === List of visibility kinds --------------------------------------------===
// ===----------------------------------------------------------------------===

#ifndef SC_VISKIND_DEF
#   define SC_VISKIND_DEF(type)
#endif

SC_VISKIND_DEF(External)
SC_VISKIND_DEF(Internal)

#undef SC_VISKIND_DEF
