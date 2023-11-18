#ifndef SDB_MODEL_EXECUTOR_H_
#define SDB_MODEL_EXECUTOR_H_

#include <filesystem>
#include <memory>
#include <sstream>

#include <svm/VirtualMachine.h>

#include "Model/Breakpoint.h"
#include "Model/Disassembler.h"
#include "Model/UIHandle.h"

namespace sdb {

/// Lists the possible states of the executor
enum class ExecState {
    ///
    Starting,

    /// Execution is running
    Running,

    /// Execution is paused
    Paused,

    ///
    Stopping,

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

    ///
    void setArguments(std::span<std::string const> arguments);

    /// Start execution
    void start();

    /// Stop execution
    void stop();

    /// Toggle execution
    void toggle();

    /// \Returns the current state
    ExecState state() const;

    ///
    bool isRunning() const { return state() == ExecState::Running; }

    ///
    bool isPaused() const { return state() == ExecState::Paused; }

    ///
    bool isStopped() const { return state() == ExecState::Stopped; }

    /// Step over the next instruction when paused
    void stepInstruction();

    /// Step over the next source line when paused
    void stepSourceLine();

    ///
    svm::VirtualMachine& VM() { return vm; }

    ///
    svm::VirtualMachine const& VM() const { return vm; }

    ///
    Disassembly& disassembly() { return disasm; }

    /// \overload
    Disassembly const& disassembly() const { return disasm; }

    ///
    std::stringstream& standardout() { return _stdout; }

    ///
    void toggleInstBreakpoint(size_t index);

    ///
    void clearBreakpoints() { breakpoints.clear(); }

    ///
    void setUIHandle(UIHandle* handle) { uiHandle = handle; }

private:
    struct ExecThread;

    ExecState starting();
    ExecState running();
    ExecState paused();
    ExecState stopping();

    ExecState doExecuteSteps();
    ExecState doStepInstruction();
    ExecState doStepSourceLine();

    std::array<uint64_t, 2> runArguments{};
    std::unique_ptr<ExecThread> execThread;
    svm::VirtualMachine vm;
    Disassembly disasm;
    UIHandle* uiHandle;
    std::mutex breakpointMutex;
    BreakpointManager breakpoints;

    std::stringstream _stdout;
};

} // namespace sdb

#endif // SDB_MODEL_EXECUTOR_H_
