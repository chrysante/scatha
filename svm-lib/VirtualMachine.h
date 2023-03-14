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

    u8 const* iptr = nullptr;
    u64* regPtr    = nullptr;
    VMFlags flags{};

    utl::vector<u64> registers;
    utl::vector<u8> memory;

    u8 const* programBreak = nullptr;
    u8* memoryPtr          = nullptr;
    u8 const* memoryBreak  = nullptr;

    size_t instructionCount = 0;
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
    void resizeMemory(size_t newSize);
    void cleanup();

    static size_t defaultRegisterCount;

private:
    utl::vector<Instruction> instructionTable;
    utl::vector<utl::vector<ExternalFunction>> extFunctionTable;
    VMStats stats;
};

} // namespace svm

#endif // SVM_VIRTUALMACHINE_H_
