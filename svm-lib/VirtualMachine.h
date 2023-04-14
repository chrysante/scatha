// PUBLIC-HEADER

#ifndef SVM_VIRTUALMACHINE_H_
#define SVM_VIRTUALMACHINE_H_

#include <span>

#include <utl/stack.hpp>
#include <utl/vector.hpp>

#include <svm/Common.h>
#include <svm/ExternalFunction.h>
#include <svm/Instruction.h>

namespace svm {

struct VMFlags {
    bool less  : 1;
    bool equal : 1;
};

/// Represents the state of an invocation of the virtual machine.
struct ExecutionContext {
    u64* regPtr    = nullptr;
    u64* bottomReg = nullptr;
    u8 const* iptr = nullptr;
    u8* stackPtr   = nullptr;
};

struct VMStats {
    size_t executedInstructions = 0;
};

struct OpCodeImpl;

/// Represents a virtual machine that allows execution of Scatha byte code.
class VirtualMachine {
public:
    /// Create a virtual machine
    VirtualMachine();

    /// Create a virtual machine with \p numRegisters number of registers and
    /// a stack of size \p stackSize
    VirtualMachine(size_t numRegisters, size_t stackSize);

    /// Load a program into memory
    void loadProgram(u8 const* data);

    /// Start execution at the program's start address
    void execute(std::span<u64 const> arguments);

    /// Start execution at \p startAddress
    void execute(size_t startAddress, std::span<u64 const> arguments);

    /// Set a slot of the external function table
    ///
    /// Slot 0 and 1 are reserved for builtin functions
    void setFunctionTableSlot(size_t slot,
                              utl::vector<ExternalFunction> functions);

    /// Access statistics
    VMStats const& getStats() const { return stats; }

    /// \Returns A view of the data in the registers of the VM
    std::span<u64 const> registerData() const { return registers; }

    /// \Returns The content of the register at index \p index
    u64 getRegister(size_t index) const { return registers[index]; }

    /// \Returns A view of the data on the stack of the VM
    std::span<u8 const> stackData() const { return stack; }

private:
    friend struct OpCodeImpl;

    utl::vector<Instruction> instructionTable;
    utl::vector<utl::vector<ExternalFunction>> extFunctionTable;

    VMFlags flags{};

    utl::vector<u64> registers;
    utl::vector<u8> text;
    utl::vector<u8> data;
    utl::vector<u8> stack;

    /// End of text section
    u8 const* programBreak = nullptr;
    /// Optional address of the `main`/`start` function.
    size_t startAddress = 0;

    /// The VM has a stack of execution contexts instead of a single one to
    /// allow nested invocations of the same program in the same VM instance via
    /// host callbacks.
    utl::stack<ExecutionContext, 4> execContexts;

    /// The currently active execution context
    ExecutionContext ctx;

    /// Statistics
    VMStats stats;
};

} // namespace svm

#endif // SVM_VIRTUALMACHINE_H_
