// clang-format off

// ===----------------------------------------------------------------------===
// === CFG -----------------------------------------------------------------===
// ===----------------------------------------------------------------------===

#ifndef SC_MIR_CFGNODE_DEF
#   define SC_MIR_CFGNODE_DEF(...)
#endif

SC_MIR_CFGNODE_DEF(Value,            VoidParent, Abstract)
SC_MIR_CFGNODE_DEF(Register,         Value,      Abstract)
SC_MIR_CFGNODE_DEF(SSARegister,      Register,   Concrete)
SC_MIR_CFGNODE_DEF(VirtualRegister,  Register,   Concrete)
SC_MIR_CFGNODE_DEF(CalleeRegister,   Register,   Concrete)
SC_MIR_CFGNODE_DEF(HardwareRegister, Register,   Concrete)
SC_MIR_CFGNODE_DEF(Constant,         Value,      Concrete)
SC_MIR_CFGNODE_DEF(UndefValue,       Value,      Concrete)
SC_MIR_CFGNODE_DEF(BasicBlock,       Value,      Concrete)
SC_MIR_CFGNODE_DEF(Callable,         Value,      Abstract)
SC_MIR_CFGNODE_DEF(Function,         Callable,   Concrete)
SC_MIR_CFGNODE_DEF(ForeignFunction,  Callable,   Concrete) // Update LAST if this changes!

#undef SC_MIR_CFGNODE_DEF

// ===----------------------------------------------------------------------===
// === Instructions --------------------------------------------------------===
// ===----------------------------------------------------------------------===

#ifndef SC_MIR_INSTCLASS_DEF
#   define SC_MIR_INSTCLASS_DEF(...)
#endif

SC_MIR_INSTCLASS_DEF(Instruction,         VoidParent,       Abstract)
SC_MIR_INSTCLASS_DEF(UnaryInstruction,    Instruction,      Abstract)
SC_MIR_INSTCLASS_DEF(StoreInst,           Instruction,      Concrete)
SC_MIR_INSTCLASS_DEF(LoadInst,            Instruction,      Concrete)
SC_MIR_INSTCLASS_DEF(CopyBase,            UnaryInstruction, Abstract)
SC_MIR_INSTCLASS_DEF(CopyInst,            CopyBase,         Concrete)
SC_MIR_INSTCLASS_DEF(CallInst,            Instruction,      Abstract)
SC_MIR_INSTCLASS_DEF(CallValueInst,       CallInst,         Concrete)
SC_MIR_INSTCLASS_DEF(CallMemoryInst,      CallInst,         Concrete)
SC_MIR_INSTCLASS_DEF(CondCopyInst,        CopyBase,         Concrete)
SC_MIR_INSTCLASS_DEF(LISPInst,            UnaryInstruction, Concrete)
SC_MIR_INSTCLASS_DEF(LEAInst,             Instruction,      Concrete)
SC_MIR_INSTCLASS_DEF(CompareInst,         Instruction,      Concrete)
SC_MIR_INSTCLASS_DEF(TestInst,            UnaryInstruction, Concrete)
SC_MIR_INSTCLASS_DEF(SetInst,             Instruction,      Concrete)
SC_MIR_INSTCLASS_DEF(UnaryArithmeticInst, UnaryInstruction, Concrete)
SC_MIR_INSTCLASS_DEF(ArithmeticInst,      Instruction,      Abstract)
SC_MIR_INSTCLASS_DEF(ValueArithmeticInst, ArithmeticInst,   Concrete)
SC_MIR_INSTCLASS_DEF(LoadArithmeticInst,  ArithmeticInst,   Concrete)
SC_MIR_INSTCLASS_DEF(ConversionInst,      UnaryInstruction, Concrete)
SC_MIR_INSTCLASS_DEF(TerminatorInst,      Instruction,      Abstract)
SC_MIR_INSTCLASS_DEF(JumpBase,            TerminatorInst,   Abstract)
SC_MIR_INSTCLASS_DEF(JumpInst,            JumpBase,         Concrete)
SC_MIR_INSTCLASS_DEF(CondJumpInst,        JumpBase,         Concrete)
SC_MIR_INSTCLASS_DEF(ReturnInst,          TerminatorInst,   Concrete)
SC_MIR_INSTCLASS_DEF(PhiInst,             Instruction,      Concrete)
SC_MIR_INSTCLASS_DEF(SelectInst,          Instruction,      Concrete) // Update LAST if this changes!

#undef SC_MIR_INSTCLASS_DEF
