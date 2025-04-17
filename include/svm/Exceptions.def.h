// clang-format off

// ===--------------------------------------------------------------------=== //
// === List of all error types -------------------------------------------=== //
// ===--------------------------------------------------------------------=== //

#ifndef SVM_EXCEPTION_DEF
#define SVM_EXCEPTION_DEF(...)
#endif

SVM_EXCEPTION_DEF(InvalidOpcodeError)
SVM_EXCEPTION_DEF(InvalidStackAllocationError)
SVM_EXCEPTION_DEF(FFIError)
SVM_EXCEPTION_DEF(InterruptException)
SVM_EXCEPTION_DEF(ArithmeticError)
SVM_EXCEPTION_DEF(MemoryAccessError)
SVM_EXCEPTION_DEF(AllocationError)
SVM_EXCEPTION_DEF(DeallocationError)
SVM_EXCEPTION_DEF(NoStartAddress)

#undef SVM_EXCEPTION_DEF
