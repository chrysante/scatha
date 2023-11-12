#ifndef SDB_MODEL_MODEL_H_
#define SDB_MODEL_MODEL_H_

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <svm/VirtualMachine.h>
#include <utl/hashtable.hpp>

#include "Model/Disassembler.h"

namespace sdb {

struct Options;

class Model {
public:
    Model();

    Model(Model const&) = delete;

    ~Model();

    ///
    void loadBinary(Options options);

    ///
    void run();

    ///
    void shutdown();

    ///
    void toggleExecution();

    ///
    void skipLine();

    ///
    void enterFunction();

    ///
    void exitFunction();

    ///
    std::span<Instruction const> instructions() const {
        return disasm.instructions();
    }

    ///
    bool isSleeping() const;

    ///
    bool isActive() const { return execThreadRunning; }

    ///
    size_t currentLine() const { return currentIndex; }

    ///
    bool isBreakpoint(size_t instIndex) const {
        std::lock_guard lock(mutex);
        return isBreakpointImpl(instIndex);
    }

    ///
    void addBreakpoint(size_t instIndex) {
        std::lock_guard lock(mutex);
        addBreakpointImpl(instIndex);
    }

    ///
    void removeBreakpoint(size_t instIndex) {
        std::lock_guard lock(mutex);
        removeBreakpointImpl(instIndex);
    }

    ///
    void toggleBreakpoint(size_t instIndex) {
        std::lock_guard lock(mutex);
        if (!isBreakpointImpl(instIndex)) {
            addBreakpointImpl(instIndex);
        }
        else {
            removeBreakpointImpl(instIndex);
        }
    }

    ///
    void clearBreakpoints() {
        std::lock_guard lock(mutex);
        breakpoints.clear();
    }

    /// \Returns the path to the currently loaded binary file
    std::filesystem::path currentFilepath() const { return _currentFilepath; }

    ///
    svm::VirtualMachine& VM() { return vm; }

    /// \overload
    svm::VirtualMachine const& VM() const { return vm; }

    ///
    std::vector<uint64_t> readRegisters(size_t numRegisters) const;

    ///
    Disassembly& disassembly() { return disasm; }

    /// \overload
    Disassembly const& disassembly() const { return disasm; }

    ///
    void setReloadCallback(std::function<void()> fn) { reloadCallback = fn; }

    ///
    void setScrollCallback(std::function<void(size_t)> fn) {
        scrollCallback = fn;
    }

    ///
    void setRefreshCallback(std::function<void()> fn) { refreshCallback = fn; }

    ///
    std::stringstream& standardout() { return _stdout; }

private:
    ///
    void startExecutionThread(std::array<uint64_t, 2> arguments);

    bool isBreakpointImpl(size_t instIndex) const {
        return breakpoints.contains(instIndex);
    }
    void addBreakpointImpl(size_t instIndex) { breakpoints.insert(instIndex); }
    void removeBreakpointImpl(size_t instIndex) {
        breakpoints.erase(instIndex);
    }
    /// Requires the VM to be currently executing
    void updateInstIndex();

    /// \Warning requires the calling thread to hold `mutex`
    bool handlePausedOrBreakpoint();

    void setScroll(size_t index);
    enum SoftOrForce { SOFT, FORCE };
    void refreshScreen(SoftOrForce = SOFT);

    enum class Signal { Sleep, Step, Run, Terminate };

    /// \Warning requires the calling thread to hold `mutex`
    void send(Signal signal);

    std::thread executionThread;
    std::condition_variable condVar;
    mutable std::mutex mutex;
    Signal signal = {};
    std::atomic<bool> execThreadRunning = false;
    std::atomic<size_t> currentIndex = 0;

    std::filesystem::path _currentFilepath;
    svm::VirtualMachine vm;
    std::vector<std::string> runArguments;
    Disassembly disasm;
    utl::hashset<size_t> breakpoints;

    std::function<void()> reloadCallback;
    std::function<void(size_t)> scrollCallback;
    std::function<void()> refreshCallback;
    std::chrono::time_point<std::chrono::steady_clock> lastRefresh =
        std::chrono::steady_clock::now();

    std::stringstream _stdout;
};

} // namespace sdb

#endif // SDB_MODEL_MODEL_H_
