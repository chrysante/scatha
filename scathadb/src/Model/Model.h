#ifndef SDB_MODEL_EXECUTOR_H_
#define SDB_MODEL_EXECUTOR_H_

#include <filesystem>
#include <memory>
#include <sstream>
#include <vector>

#include <svm/VirtualMachine.h>

#include "Model/Breakpoint.h"
#include "Model/Disassembler.h"
#include "Model/UIHandle.h"

namespace sdb {

/// Lists the possible states of the executor
enum class ExecState {
    /// Starting program execution
    Starting,

    /// Execution is running
    Running,

    /// Execution is paused
    Paused,

    /// Interrupting execution
    Stopping,

    /// Gracefully exited program execution
    Exiting,

    ///
    Stopped,
};

///
class Model {
public:
    explicit Model(UIHandle* uiHandle = nullptr);

    ~Model();

    /// Load the program at \p filepath into the VM.
    /// This unloads the currently active program
    void loadProgram(std::filesystem::path filepath);

    /// Unload the currently active program
    void unloadProgram();

    /// Set the arguments that will be loaded into VM stack memory before
    /// execution starts
    void setArguments(std::vector<std::string> arguments);

    /// Start execution
    void start();

    /// Stop execution
    void stop();

    /// Toggle execution
    void toggle();

    /// \Returns the current state
    ExecState state() const;

    /// \Returns `state() == ExecState::Running`
    bool isRunning() const { return state() == ExecState::Running; }

    /// \Returns `state() == ExecState::Paused`
    bool isPaused() const { return state() == ExecState::Paused; }

    /// \Returns `state() == ExecState::Stopped`
    bool isStopped() const { return state() == ExecState::Stopped; }

    /// Step over the next instruction when paused
    void stepInstruction();

    /// Step over the next source line when paused
    void stepSourceLine();

    ///
    [[nodiscard]] std::unique_lock<std::mutex> lockVM() {
        return std::unique_lock(vmMutex);
    }

    /// \Returns a reference to the VM
    svm::VirtualMachine& VM() { return vm; }

    /// \overload
    svm::VirtualMachine const& VM() const { return vm; }

    /// \Returns a reference to the disassembled program
    Disassembly& disassembly() { return disasm; }

    /// \overload
    Disassembly const& disassembly() const { return disasm; }

    /// \Returns the standard-out stream
    std::stringstream& standardout() { return _stdout; }

    ///
    void toggleInstBreakpoint(size_t index) { breakpoints.toggle(index); }

    ///
    void clearBreakpoints() { breakpoints.clear(); }

    ///
    bool hasInstBreakpoint(size_t index) const { return breakpoints.at(index); }

    ///
    void setUIHandle(UIHandle* handle) { uiHandle = handle; }

    /// Read \p count many registers out of the VM starting from index 0 (for
    /// now)
    std::vector<uint64_t> readRegisters(size_t count);

private:
    struct ExecThread;

    ExecState starting();
    ExecState running();
    ExecState paused();
    ExecState stopping();
    ExecState exiting();

    ExecState doExecuteSteps();
    ExecState doStepInstruction();
    ExecState doStepSourceLine();

    void handleInstEncounter(size_t binaryOffset, BreakState state);

    void handleException();

    std::vector<std::string> runArguments;
    std::unique_ptr<ExecThread> execThread;
    std::mutex vmMutex;
    svm::VirtualMachine vm;
    Disassembly disasm;
    UIHandle* uiHandle;
    std::mutex breakpointMutex;
    BreakpointSet breakpoints;

    std::stringstream _stdout;
};

} // namespace sdb

#endif // SDB_MODEL_EXECUTOR_H_
