#ifndef SDB_MODEL_EVENTS_H_
#define SDB_MODEL_EVENTS_H_

#include <functional>

#include <scdis/Disassembly.h>
#include <svm/Exceptions.h>

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

/// Sent on the executor thread before stepping a source line
struct WillStepSourceLine {
    svm::VirtualMachine& vm;
    scdis::InstructionPointerOffset ipo;
};

///
struct DidStepSourceLine {
    svm::VirtualMachine& vm;
    scdis::InstructionPointerOffset ipo;
    bool* isReturn = nullptr;
};

/// Sent if UI must be reconstructed. For now this is only sent when a patient
/// program is loaded.
struct ReloadUIRequest {};

enum class BreakState : unsigned {
    None,
    Paused,
    Step,
    Breakpoint,
    Error,
};

/// Sent if execution is interrupted
struct BreakEvent {
    /// The instruction pointer offset where the break occurred
    scdis::InstructionPointerOffset ipo;

    /// The reason for the break
    BreakState state;

    /// Optionally the exception causing the break
    svm::ExceptionVariant exception = {};
};

/// Sent by the executor if an exception is thrown when starting execution
struct PatientStartFailureEvent {
    svm::ExceptionVariant exception;
};

/// Sent when the patient program writes a newline character to the terminal
struct PatientConsoleOutputEvent {};

/// Sent if a source file shall be openend
struct OpenSourceFileRequest {
    size_t fileIndex;
};

} // namespace sdb

#endif // SDB_MODEL_EVENTS_H_
