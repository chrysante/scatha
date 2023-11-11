#ifndef SDB_PROGRAM_H_
#define SDB_PROGRAM_H_

#include <atomic>
#include <chrono>
#include <functional>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <svm/VirtualMachine.h>
#include <utl/hashtable.hpp>

#include "Disassembler.h"

namespace sdb {

class Model {
public:
    explicit Model(svm::VirtualMachine vm,
                   uint8_t const* program,
                   std::array<uint64_t, 2> arguments);

    Model(Model const&) = delete;

    ~Model();

    ///
    void startExecutionThread();

    ///
    void toggleExecution();

    ///
    void skipLine();

    ///
    void enterFunction();

    ///
    void exitFunction();

    ///
    void run();

    ///
    void shutdown();

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
    bool isBreakpoint(size_t line) const {
        std::lock_guard lock(mutex);
        return isBreakpointImpl(line);
    }

    ///
    void addBreakpoint(size_t line) {
        std::lock_guard lock(mutex);
        addBreakpointImpl(line);
    }

    ///
    void removeBreakpoint(size_t line) {
        std::lock_guard lock(mutex);
        removeBreakpointImpl(line);
    }

    ///
    void toggleBreakpoint(size_t line) {
        std::lock_guard lock(mutex);
        if (!isBreakpointImpl(line)) {
            addBreakpointImpl(line);
        }
        else {
            removeBreakpointImpl(line);
        }
    }

    ///
    svm::VirtualMachine& virtualMachine() { return vm; }

    /// \overload
    svm::VirtualMachine const& virtualMachine() const { return vm; }

    ///
    std::vector<uint64_t> readRegisters(size_t numRegisters) const;

    ///
    Disassembly& disassembly() { return disasm; }

    /// \overload
    Disassembly const& disassembly() const { return disasm; }

    ///
    void setScrollCallback(std::function<void(size_t)> fn) {
        scrollCallback = fn;
    }

    ///
    void setRefreshCallback(std::function<void()> fn) { refreshCallback = fn; }

    ///
    std::stringstream& standardout() { return _stdout; }

private:
    bool isBreakpointImpl(size_t line) const {
        return breakpoints.contains(line);
    }
    void addBreakpointImpl(size_t line) { breakpoints.insert(line); }
    void removeBreakpointImpl(size_t line) { breakpoints.erase(line); }
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

    svm::VirtualMachine vm;
    Disassembly disasm;
    std::array<uint64_t, 2> arguments;
    utl::hashset<size_t> breakpoints;

    std::function<void(size_t)> scrollCallback;

    std::function<void()> refreshCallback;
    std::chrono::time_point<std::chrono::steady_clock> lastRefresh =
        std::chrono::steady_clock::now();

    std::stringstream _stdout;
};

} // namespace sdb

#endif // SDB_PROGRAM_H_
