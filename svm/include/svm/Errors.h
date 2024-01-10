#ifndef SVM_ERRORS_H_
#define SVM_ERRORS_H_

#include <concepts>
#include <stdexcept>
#include <string>
#include <variant>

#include <svm/Common.h>
#include <svm/VirtualPointer.h>

namespace svm {

/// Thrown if unknown opcode is encountered
class InvalidOpcodeError {
public:
    explicit InvalidOpcodeError(u64 value): val(value) {}

    /// The value of the invalid opcode
    u64 value() const { return val; }

    ///
    std::string message() const;

private:
    u64 val;
};

///
class InvalidStackAllocationError {
public:
    explicit InvalidStackAllocationError(u64 count): cnt(count) {}

    /// The number of allocated bytes
    u64 count() const { return cnt; }

    ///
    std::string message() const;

private:
    u64 cnt;
};

///
class FFIError {
public:
    enum Reason {
        ///
        FailedToInit
    };

    explicit FFIError(Reason reason, std::string functionName):
        _reason(reason), funcName(std::move(functionName)) {}

    ///
    Reason reason() const { return _reason; }

    ///
    std::string const& functionName() const { return funcName; }

    ///
    std::string message() const;

private:
    Reason _reason;
    std::string funcName;
};

/// Error class thrown when executing trap instruction
class TrapError {
public:
    TrapError() = default;

    ///
    std::string message() const;
};

/// Common base class of all memory errors
class MemoryError {
public:
    /// The pointer of the invalid memory operation
    VirtualPointer pointer() const { return ptr; }

protected:
    explicit MemoryError(VirtualPointer pointer): ptr(pointer) {}

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

    MemoryAccessError(Reason reason, VirtualPointer ptr, size_t size):
        MemoryError(ptr), _reason(reason), _size(size) {}

    /// \Returns the reason why the memory access failed
    Reason reason() const { return _reason; }

    /// \Returns the size of the block that was accessed
    size_t size() const { return _size; }

    ///
    std::string message() const;

private:
    Reason _reason;
    size_t _size;
};

/// Invalid arguments passed to `allocate()`
class AllocationError: public MemoryError {
public:
    enum Reason { InvalidSize, InvalidAlign };

    AllocationError(Reason reason, size_t size, size_t align):
        MemoryError({}), _reason(reason), _size(size), _align(align) {}

    /// \Returns the reason the allocation failed
    Reason reason() const { return _reason; }

    /// \Returns the size passed to `allocate()`
    size_t size() const { return _size; }

    /// \Returns the align passed to `allocate()`
    size_t align() const { return _align; }

    ///
    std::string message() const;

private:
    Reason _reason;
    size_t _size;
    size_t _align;
};

/// Tried to deallocate a block that has not been allocated before
class DeallocationError: public MemoryError {
public:
    DeallocationError(VirtualPointer ptr, size_t size, size_t align):
        MemoryError(ptr), _size(size), _align(align) {}

    /// The size of the block passed to `deallocate()`
    size_t size() const { return _size; }

    /// The align of the block passed to `deallocate()`
    size_t align() const { return _align; }

    ///
    std::string message() const;

private:
    size_t _size;
    size_t _align;
};

/// Variant of all concrete error classes
class ErrorVariant:
    public std::variant<std::monostate
#define SVM_ERROR_DEF(Name) , Name
#include <svm/Errors.def>
                        > {
public:
    using variant::variant;

    ///
    std::string message() const;

    /// Returns true if this error variant holds a runtime error
    bool hasError() const {
        return !std::holds_alternative<std::monostate>(*this);
    }
};

/// Exception class
class RuntimeException: public std::runtime_error {
public:
    explicit RuntimeException(ErrorVariant error):
        runtime_error(error.message()), err(std::move(error)) {}

    /// \Returns the wrapped error object
    ErrorVariant const& error() const& { return err; }
    /// \overload
    ErrorVariant& error() & { return err; }
    /// \overload
    ErrorVariant const&& error() const&& { return std::move(err); }
    /// \overload
    ErrorVariant&& error() && { return std::move(err); }

private:
    ErrorVariant err;
};

/// \Throws `Err` constructed by \p args... wrapped in a `RuntimeException`
template <typename Err, typename... Args>
    requires std::constructible_from<Err, Args...> &&
             std::constructible_from<ErrorVariant, Err>
[[noreturn]] SVM_NOINLINE void throwError(Args&&... args) {
    throw RuntimeException(Err(std::forward<Args>(args)...));
}

} // namespace svm

#endif // SVM_ERRORS_H_
