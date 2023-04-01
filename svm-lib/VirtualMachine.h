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

struct VMState {
    VMState()               = default;
    VMState(VMState const&) = delete;
    VMState(VMState&&)      = default;

    u64* regPtr    = nullptr;
    u8 const* iptr = nullptr;
    u8* stackPtr   = nullptr;
    VMFlags flags{};

    utl::vector<u64> registers;
    utl::vector<u8> text;
    utl::vector<u8> data;
    utl::vector<u8> stack;

    u8 const* programBreak  = nullptr;
    size_t instructionCount = 0;
    size_t programStart     = 0;
};

struct VMStats {
    size_t executedInstructions = 0;
};

struct OpCodeImpl;

class VirtualMachine: VMState {
public:
    VirtualMachine();
    void loadProgram(u8 const* data);
    void execute();

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

private:
    void cleanup();

    static size_t defaultRegisterCount;

    static size_t defaultStackSize;

private:
    utl::vector<Instruction> instructionTable;
    utl::vector<utl::vector<ExternalFunction>> extFunctionTable;
    VMStats stats;
};

} // namespace svm

#endif // SVM_VIRTUALMACHINE_H_
