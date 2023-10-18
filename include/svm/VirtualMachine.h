#ifndef SVM_VIRTUALMACHINE_H_
#define SVM_VIRTUALMACHINE_H_

#include <memory>
#include <span>
#include <vector>

#include <svm/Common.h>
#include <svm/ExternalFunction.h>

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

    /// # Memory

    /// Allocates a memory region in the current stack frame.
    uint64_t allocateStackMemory(size_t numBytes);

    /// Allocates virtual memory on the heap
    uint64_t allocateMemory(size_t size, size_t align);

    /// Deallocates memory allocated with `allocateMemory()`
    void deallocateMemory(uint64_t ptr, size_t size, size_t align);

    /// Converts an opaque virtual pointer into a raw pointer.
    /// The pointer must be dereferenceable at \p numBytes bytes
    void* derefPointer(uint64_t ptr, size_t numBytes) const;

    /// Converts a virtual pointer into a reference to type `T`
    template <typename T>
    T& derefPointer(uint64_t ptr) const {
        return *reinterpret_cast<T*>(derefPointer(ptr, sizeof(T)));
    }

    /// Print the values in the first \p n registers of the current execution
    /// frame
    void printRegisters(size_t n) const;

    /// This is not private because many internal outside of this class
    /// reference this but it is effectively private because the type `VMImpl`
    /// is internal
    std::unique_ptr<VMImpl> impl;
};

} // namespace svm

#endif // SVM_VIRTUALMACHINE_H_
