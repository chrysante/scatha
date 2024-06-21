// clang-format off

// ===--------------------------------------------------------------------=== //
// === List of all entity types ------------------------------------------=== //
// ===--------------------------------------------------------------------=== //

#ifndef SVM_ERROR_DEF
#define SVM_ERROR_DEF(...)
#endif

SVM_ERROR_DEF(InvalidOpcodeError)
SVM_ERROR_DEF(InvalidStackAllocationError)
SVM_ERROR_DEF(FFIError)
SVM_ERROR_DEF(TrapError)
SVM_ERROR_DEF(ArithmeticError)
SVM_ERROR_DEF(MemoryAccessError)
SVM_ERROR_DEF(AllocationError)
SVM_ERROR_DEF(DeallocationError)
SVM_ERROR_DEF(NoStartAddress)

#undef SVM_ERROR_DEF
