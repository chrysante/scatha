#ifndef SDB_MODEL_MODEL_H_
#define SDB_MODEL_MODEL_H_

#include <filesystem>
#include <memory>
#include <mutex>
#include <sstream>
#include <vector>

#include <scdis/Disassembly.h>
#include <svm/VirtualMachine.h>

#include <scathadb/Model/BreakpointManager.h>
#include <scathadb/Model/Executor.h>
#include <scathadb/Model/SourceDebugInfo.h>
#include <scathadb/Model/Stdout.h>
#include <scathadb/Util/Messenger.h>

namespace sdb {

///
class Model {
public:
    explicit Model(std::shared_ptr<Messenger> _messenger);

    /// Load the program at \p filepath into the VM.
    /// This unloads the currently active program
    void loadProgram(std::filesystem::path filepath);

    /// Unload the currently active program
    void unloadProgram();

    ///
    bool isProgramLoaded() const;

    /// Set the arguments that will be loaded into VM stack memory before
    /// execution starts
    void setArguments(std::vector<std::string> arguments);

    /// \Returns the file path of the currently loaded executable
    std::filesystem::path currentFilepath() const { return _currentFilepath; }

    ///
    void startExecution();

    ///
    void toggleExecution() { executor.toggleExecution(); }

    ///
    void stopExecution() { executor.stopExecution(); }

    /// Step over the next instruction when paused
    void stepInstruction() { executor.stepInstruction(); }

    /// Step over the next source line when paused
    void stepSourceLine() { executor.stepSourceLine(); }

    ///
    void stepOut() { executor.stepOut(); }

    ///
    bool isIdle() const { return executor.isIdle(); }

    ///
    bool isPaused() const { return executor.isPaused(); }

    /// \Returns a locked read-only reference to the VM
    Locked<svm::VirtualMachine const&> readVM() { return executor.readVM(); }

    /// \Returns a reference to the disassembled program
    scdis::Disassembly& disassembly() { return disasm; }

    /// \overload
    scdis::Disassembly const& disassembly() const { return disasm; }

    /// \Returns the standard-out stream
    CallbackStringStream const& standardout() const { return _stdout; }

    ///
    void toggleInstBreakpoint(size_t instIndex);

    /// \Returns `true` if a breakpoint could be set on line \p lineIndex
    bool toggleSourceBreakpoint(SourceLine lineIndex);

    ///
    bool hasInstBreakpoint(size_t instIndex) const;

    ///
    bool hasSourceBreakpoint(SourceLine lineIndex) const;

    ///
    void clearBreakpoints();

    ///
    SourceDebugInfo const& sourceDebug() const { return sourceDbg; }

private:
    std::shared_ptr<Messenger> _messenger;
    std::filesystem::path _currentFilepath;
    std::vector<std::string> runArguments;
    Executor executor;
    scdis::Disassembly disasm;
    SourceDebugInfo sourceDbg;
    BreakpointManager breakpointManager;

    CallbackStringStream _stdout;
};

} // namespace sdb

#endif // SDB_MODEL_MODEL_H_
