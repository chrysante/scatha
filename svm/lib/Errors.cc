#include "svm/Errors.h"

#include <utl/strcat.hpp>
#include <utl/utility.hpp>

using namespace svm;

std::string InvalidOpcodeError::message() const {
    return utl::strcat("Executed invalid opcode: ", value());
}

std::string InvalidStackAllocationError::message() const {
    return utl::strcat("Invalid stack allocation of ", count(), " bytes");
}

std::string FFIError::message() const {
    switch (reason()) {
    case FailedToInit:
        return utl::strcat("Failed to initialize foreign function \"",
                           functionName(), "\"");
    }
    unreachable();
}

std::string TrapError::message() const { return "Executed trap instruction"; }

std::string ArithmeticError::message() const {
    return "Attempt to divide by zero";
}

std::string MemoryAccessError::message() const {
    switch (reason()) {
    case MemoryNotAllocated:
        return utl::strcat("Accessed unallocated memory at address ",
                           pointer());
    case DerefRangeTooBig:
        return utl::strcat("Dereferenced pointer ", pointer(), " at ", size(),
                           " bytes outside its valid range");
    case MisalignedLoad:
        return utl::strcat("Misaligned load of address ", pointer());
    case MisalignedStore:
        return utl::strcat("Misaligned store of address ", pointer());
    }
    unreachable();
}

std::string AllocationError::message() const {
    return utl::strcat("Invalid heap allocation of ", size(),
                       " bytes with alignment ", align());
}

std::string DeallocationError::message() const {
    return utl::strcat("Tried to deallocate ", size(), " bytes at address ",
                       pointer(), " that have not been allocated before");
}

std::string NoStartAddress::message() const {
    return "Attempted execution without start address";
}

std::string ErrorVariant::message() const {
    utl::overload callback{
        [](auto const& e) { return e.message(); },
        [](std::monostate) -> std::string { return "No error"; },
    };
    return std::visit(callback, *this);
}
