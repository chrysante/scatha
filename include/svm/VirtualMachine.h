#ifndef SVM_VIRTUALMACHINE_H_
#define SVM_VIRTUALMACHINE_H_

#include <span>

#include <utl/stack.hpp>
#include <utl/vector.hpp>

#include <svm/Common.h>
#include <svm/ExternalFunction.h>

namespace svm {

struct VMFlags {
    bool less  : 1;
    bool equal : 1;
};

/// Represents the state of an invocation of the virtual machine.
struct ExecutionFrame {
    u64* regPtr = nullptr;
    u64* bottomReg = nullptr;
    u8 const* iptr = nullptr;
    u8* stackPtr = nullptr;
};

struct VMStats {
    size_t executedInstructions = 0;
};

struct OpCodeImpl;

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

    /// Create a virtual machine with \p numRegisters number of registers and
    /// a stack of size \p stackSize
    VirtualMachine(size_t numRegisters, size_t stackSize);

    /// Load a program into memory
    void loadBinary(u8 const* data);

    /// Allocates a memory region in the current stack frame.
    u8* allocateStackMemory(size_t numBytes);

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
                              utl::vector<ExternalFunction> functions);

    /// Set the entry at index \p index of the slot \p slot of the external
    /// function table
    void setFunction(size_t slot, size_t index, ExternalFunction function);

    /// Access statistics
    VMStats const& getStats() const { return stats; }

    /// \Returns A view of the data in the registers of the VM
    std::span<u64 const> registerData() const { return registers; }

    /// \Returns The content of the register at index \p index
    u64 getRegister(size_t index) const { return registers[index]; }

    /// \Returns A view of the data on the stack of the VM
    std::span<u8 const> stackData() const { return stack; }

private:
    /// Print the values in the first \p n registers of the current execution
    /// frame
    void printRegisters(size_t n) const;

    utl::vector<utl::vector<ExternalFunction>> extFunctionTable;

    VMFlags flags{};

    /// Executable data and code
    utl::vector<u8> binary;
    /// Memory for registers
    utl::vector<u64> registers;
    /// Stack memory
    utl::vector<u8> stack;

    /// Begin of the executable section
    u8 const* text;
    /// End of text section
    u8 const* programBreak = nullptr;

    /// Optional address of the `main`/`start` function.
    size_t startAddress = 0;

    /// The VM has a stack of execution contexts instead of a single one to
    /// allow nested invocations of the same program in the same VM instance via
    /// host callbacks.
    utl::stack<ExecutionFrame, 4> execFrames;

    /// The currently active execution frame
    ExecutionFrame currentFrame;

    /// Statistics
    VMStats stats;
};

} // namespace svm

#endif // SVM_VIRTUALMACHINE_H_
