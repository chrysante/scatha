// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_VM_VIRTUALMACHINE_H_
#define SCATHA_VM_VIRTUALMACHINE_H_

#include <utl/stack.hpp>
#include <utl/vector.hpp>

#include <scatha/Basic/Basic.h>
#include <scatha/VM/ExternalFunction.h>
#include <scatha/VM/Instruction.h>
#include <scatha/VM/Program.h>

namespace scatha::vm {

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

class SCATHA(API) VirtualMachine: VMState {
public:
    VirtualMachine();
    void load(Program const&);
    void execute();

    void addExternalFunction(size_t slot, ExternalFunction);

    void setFunctionTableSlot(size_t slot, utl::vector<ExternalFunction> functions);

    VMState const& getState() const { return static_cast<VMState const&>(*this); }
    VMStats const& getStats() const { return stats; }

    friend struct OpCodeImpl;

    static void setDefaultRegisterCount(size_t count) { defaultRegisterCount = count; }

private:
    void resizeMemory(size_t newSize);
    void cleanup();

    static size_t defaultRegisterCount;

private:
    utl::vector<Instruction> instructionTable;
    utl::vector<utl::vector<ExternalFunction>> extFunctionTable;
    VMStats stats;
};

} // namespace scatha::vm

#endif // SCATHA_VM_VIRTUALMACHINE_H_
