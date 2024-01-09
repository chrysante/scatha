#ifndef SVM_ERRORS_H_
#define SVM_ERRORS_H_

#include <concepts>
#include <stdexcept>
#include <string>
#include <variant>

#include <svm/Common.h>
#include <svm/VirtualPointer.h>

namespace svm {

/// Common base class of all runtime errors
class RuntimeError: std::runtime_error {
protected:
    explicit RuntimeError(std::string message):
        std::runtime_error(std::move(message)) {}
};

/// Thrown if unknown opcode is encountered
class InvalidOpcodeError: public RuntimeError {
public:
    explicit InvalidOpcodeError(u64 value);

    /// The value of the invalid opcode
    u64 value() const { return val; }

private:
    u64 val;
};

///
class InvalidStackAllocationError: public RuntimeError {
public:
    explicit InvalidStackAllocationError(u64 count);

    /// The number of allocated bytes
    u64 count() const { return cnt; }

private:
    u64 cnt;
};

///
class FFIError: public RuntimeError {
public:
    enum Reason {
        ///
        FailedToInit
    };

    explicit FFIError(Reason reason, std::string functionName):
        RuntimeError("FFIError"),
        _reason(reason),
        funcName(std::move(functionName)) {}

    ///
    Reason reason() const {
        return _reason;
    }

    ///
    std::string const& functionName() const { return funcName; }

private:
    Reason _reason;
    std::string funcName;
};

/// Error class thrown when executing trap instruction
class TrapError: public RuntimeError {
public:
    TrapError(): RuntimeError("Executed trap instruction") {}
};

/// Common base class of all memory errors
class MemoryError: public RuntimeError {
public:
    /// The pointer of the invalid memory operation
    VirtualPointer pointer() const { return ptr; }

protected:
    explicit MemoryError(std::string message, VirtualPointer pointer):
        RuntimeError(std::move(message)), ptr(pointer) {}

private:
    VirtualPointer ptr;
};

/// Invalid memory access
class MemoryAccessError: public MemoryError {
public:
    enum Reason {
        /// Tried to dereference a pointer that has not been allocated before
        MemoryNotAllocated,

        /// Tried to dereference a pointer beyond its valid range
        DerefRangeTooBig,

        ///
        MisalignedLoad,

        ///
        MisalignedStore,
    };

    MemoryAccessError(Reason, VirtualPointer ptr, size_t size):
        MemoryError("MemoryAccessError", ptr), _size(size) {}

    /// \Returns the reason why the memory access failed
    Reason reason() const { return _reason; }

    /// \Returns the size of the block that was accessed
    size_t size() const { return _size; }

private:
    Reason _reason;
    size_t _size;
};

/// Invalid arguments passed to `allocate()`
class AllocationError: public MemoryError {
public:
    enum Reason { InvalidSize, InvalidAlign };

    AllocationError(Reason reason, size_t size, size_t align):
        MemoryError("AllocationError", {}),
        _reason(reason),
        _size(size),
        _align(align) {}

    /// \Returns the reason the allocation failed
    Reason reason() const { return _reason; }

    /// \Returns the size passed to `allocate()`
    size_t size() const { return _size; }

    /// \Returns the align passed to `allocate()`
    size_t align() const { return _align; }

private:
    Reason _reason;
    size_t _size;
    size_t _align;
};

/// Tried to deallocate a block that has not been allocated before
class DeallocationError: public MemoryError {
public:
    DeallocationError(VirtualPointer ptr, size_t size, size_t align):
        MemoryError("DeallocationError", ptr), _size(size), _align(align) {}

    /// The size of the block passed to `deallocate()`
    size_t size() const { return _size; }

    /// The align of the block passed to `deallocate()`
    size_t align() const { return _align; }

private:
    size_t _size;
    size_t _align;
};

/// \Throws `Err` constructed by \p args... wrapped in a `RuntimeException`
template <std::derived_from<RuntimeError> Err, typename... Args>
    requires std::constructible_from<Err, Args...>
[[noreturn]] SVM_NOINLINE void throwError(Args&&... args) {
    throw Err(Err(std::forward<Args>(args)...));
}

} // namespace svm

#endif // SVM_ERRORS_H_
