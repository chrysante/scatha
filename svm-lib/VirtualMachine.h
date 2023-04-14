// PUBLIC-HEADER

#ifndef SVM_VIRTUALMACHINE_H_
#define SVM_VIRTUALMACHINE_H_

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

struct ExecutionContext {
    u64* regPtr    = nullptr;
    u64* bottomReg = nullptr;
    u8 const* iptr = nullptr;
    u8* stackPtr   = nullptr;
};

struct VMState {
    VMState()               = default;
    VMState(VMState const&) = delete;
    VMState(VMState&&)      = default;

    VMFlags flags{};

    utl::vector<u64> registers;
    utl::vector<u8> text;
    utl::vector<u8> data;
    utl::vector<u8> stack;

    /// End of text section
    u8 const* programBreak  = nullptr;
    size_t instructionCount = 0;

    utl::stack<ExecutionContext> execContexts;
    ExecutionContext ctx;
};

struct VMStats {
    size_t executedInstructions = 0;
};

struct OpCodeImpl;

class VirtualMachine: VMState {
public:
    VirtualMachine();
    void loadProgram(u8 const* data);
    void execute(size_t start);

    void addExternalFunction(size_t slot, ExternalFunction);

    void setFunctionTableSlot(size_t slot,
                              utl::vector<ExternalFunction> functions);

    VMState const& getState() const {
        return static_cast<VMState const&>(*this);
    }

    VMStats const& getStats() const { return stats; }

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
    VMStats stats;
};

} // namespace svm

#endif // SVM_VIRTUALMACHINE_H_
