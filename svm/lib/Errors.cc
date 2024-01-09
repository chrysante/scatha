#include "svm/Errors.h"

#include <utl/strcat.hpp>

using namespace svm;

InvalidOpcodeError::InvalidOpcodeError(u64 value):
    RuntimeError(utl::strcat("Executed invalid opcode: ", value)), val(value) {}

InvalidStackAllocationError::InvalidStackAllocationError(u64 count):
    RuntimeError(utl::strcat("Invalid stack allocation size: ", count)),
    cnt(count) {}
