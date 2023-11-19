#ifndef SVM_VIRTUALMACHINE_H_
#define SVM_VIRTUALMACHINE_H_

#include <memory>
#include <span>
#include <vector>

#include <svm/Common.h>
#include <svm/ExternalFunction.h>
#include <svm/VMData.h>
#include <svm/VirtualPointer.h>

namespace svm {

struct VMImpl;

/// Represents a virtual machine that allows execution of Scatha byte code.
class VirtualMachine {
public:
    /// The default number of registers of an instance of `VirtualMachine`.
    static constexpr size_t DefaultRegisterCount = 1 << 20;

    /// The default stack size of an instance of `VirtualMachine`.
    static constexpr size_t DefaultStackSize = 1 << 20;

    /// The number of registers available to a single call frame
    static constexpr size_t MaxCallframeRegisterCount = 256;

    /// Create a virtual machine
    VirtualMachine();

    /// Lifetime @{
    VirtualMachine(VirtualMachine&&) noexcept;
    VirtualMachine& operator=(VirtualMachine&&) noexcept;
    ~VirtualMachine();
    /// @}

    /// Create a virtual machine with \p numRegisters number of registers and
    /// a stack of size \p stackSize
    VirtualMachine(size_t numRegisters, size_t stackSize);

    /// Load a program into memory
    void loadBinary(u8 const* data);

    /// Start execution at the program's start address
    u64 const* execute(std::span<u64 const> arguments);

    /// Start execution at \p startAddress
    /// \Returns Bottom register pointer of the run execution frame
    u64 const* execute(size_t startAddress, std::span<u64 const> arguments);

    /// # Stepwise execution / debugger implementation
    /// @{
    /// Start stepwise execution of the loaded program
    void beginExecution(std::span<u64 const> arguments);

    /// \overload
    void beginExecution(size_t startAddress, std::span<u64 const> arguments);

    /// \Returns `true` if the current execution of the machine is runnning
    bool running() const;

    /// Execute one instruction.
    /// \pre `beginExecution()` has been called and `running()` returns `true`
    void stepExecution();

    /// Ends stepwise execution
    /// \pre `running()` returns `false`
    u64 const* endExecution();

    /// \Returns the instruction pointer offset from the beginning of the binary
    size_t instructionPointerOffset() const;

    /// Set the instruction pointer to \p offset bytes past the beginning of the
    /// static data section
    void setInstructionPointerOffset(size_t offset);

    /// @} Stepwise execution

    /// Set a slot of the external function table
    ///
    /// Slot 0 and 1 are reserved for builtin functions
    /// Note that any entries defined by calls to \p setFunction the the same
    /// slot are overwritten
    void setFunctionTableSlot(size_t slot,
                              std::vector<ExternalFunction> functions);

    /// Set the entry at index \p index of the slot \p slot of the external
    /// function table
    void setFunction(size_t slot, size_t index, ExternalFunction function);

    /// \Returns A view of the data in the registers of the VM
    std::span<u64 const> registerData() const;

    /// \Returns The content of the register at index \p index
    u64 getRegister(size_t index) const;

    /// \Returns A view of the data on the stack of the VM
    std::span<u8 const> stackData() const;

    ///
    CompareFlags getCompareFlags() const;

    ///
    ExecutionFrame getCurrentExecFrame() const;

    /// # Memory

    /// Allocates a memory region in the current stack frame.
    VirtualPointer allocateStackMemory(size_t numBytes, size_t align);

    /// Allocates virtual memory on the heap
    VirtualPointer allocateMemory(size_t size, size_t align);

    /// Deallocates memory allocated with `allocateMemory()`
    void deallocateMemory(VirtualPointer ptr, size_t size, size_t align);

    /// \Returns the number of bytes at which the pointer \p ptr is
    /// dereferencable. If the pointer is not valid a negative number is
    /// returned
    ptrdiff_t validPtrRange(VirtualPointer ptr) const;

    /// Converts an opaque virtual pointer into a raw pointer.
    /// The pointer must be dereferenceable at \p numBytes bytes
    void* derefPointer(VirtualPointer ptr, size_t numBytes) const;

    /// Converts a virtual pointer into a reference to type `T`
    template <typename T>
    T& derefPointer(VirtualPointer ptr) const {
        return *reinterpret_cast<T*>(derefPointer(ptr, sizeof(T)));
    }

    /// Print the values in the first \p n registers of the current execution
    /// frame
    void printRegisters(size_t n) const;

    /// Set the standard input and output streams to \p in and \p out
    /// If any of the arguments is null the respective stream is not modified
    void setIOStreams(std::istream* in, std::ostream* out);

    /// \Returns the currently set input stream
    std::istream& istream() const;

    /// \Returns the currently set output stream
    std::ostream& ostream() const;

    /// This is not private because many internals outside of this class
    /// reference this but it is effectively private because the type `VMImpl`
    /// is internal
    std::unique_ptr<VMImpl> impl;
};

} // namespace svm

#endif // SVM_VIRTUALMACHINE_H_
