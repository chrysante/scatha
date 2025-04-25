#ifndef SCATHADB_MODEL_EVENTS_H_
#define SCATHADB_MODEL_EVENTS_H_

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
struct DoOnVMThread {
    std::function<void(svm::VirtualMachine&)> callback;
};

/// Sent by the executor after the patient process terminated execution
struct ProcessTerminated {};

/// Sent by the executor after the patient process is killed
struct ProcessKilled {};

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

#endif // SCATHADB_MODEL_EVENTS_H_
