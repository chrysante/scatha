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

class VirtualMachine {
public:
    VirtualMachine();
    void loadProgram(u8 const* data);

    /// Start execution at the program's start address.
    void execute(std::span<u64 const> arguments);

    /// Start execution at \p startAddress
    void execute(size_t startAddress, std::span<u64 const> arguments);

    void addExternalFunction(size_t slot, ExternalFunction);

    void setFunctionTableSlot(size_t slot,
                              utl::vector<ExternalFunction> functions);

    VMStats const& getStats() const { return stats; }

    std::span<u64 const> registerData() const { return registers; }

    u64 getRegister(size_t index) const { return registers[index]; }

    std::span<u8 const> stackData() const { return stack; }

    friend struct OpCodeImpl;

    static void setDefaultRegisterCount(size_t count) {
        defaultRegisterCount = count;
    }

    static void setDefaultStackSize(size_t numBytes) {
        defaultStackSize = numBytes;
    }

private:
    static size_t defaultRegisterCount;

    static size_t defaultStackSize;

private:
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
