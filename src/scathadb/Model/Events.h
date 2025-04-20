#ifndef SDB_MODEL_EVENTS_H_
#define SDB_MODEL_EVENTS_H_

#include <functional>

#include <scdis/Disassembly.h>

namespace svm {
class VirtualMachine;
}

namespace sdb {

/// Sent on the executor thread right before patient execution starts
struct WillBeginExecution {
    svm::VirtualMachine& vm;
};

/// Sends an action to the executor to be immediately applied on the interrupted
/// VM
struct DoInterruptedOnVM {
    std::function<void(svm::VirtualMachine&)> callback;
};

/// The executor listens to these messages and sets `*value` to true if it's
/// idle
/// TODO: Evaluate if we need this
struct IsExecIdle {
    bool* value;
};

/// Sent on the executor thread before stepping an instruction
struct WillStepInstruction {
    svm::VirtualMachine& vm;
    scdis::InstructionPointerOffset ipo;
};

/// Sent on the executor thread after stepping an instruction
struct DidStepInstruction {
    svm::VirtualMachine& vm;
    scdis::InstructionPointerOffset ipo;
};

} // namespace sdb

#endif // SDB_MODEL_EVENTS_H_
