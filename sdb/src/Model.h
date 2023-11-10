#ifndef SDB_PROGRAM_H_
#define SDB_PROGRAM_H_

#include <atomic>
#include <optional>
#include <span>
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
    std::span<Instruction const> instructions() const {
        return disasm.instructions();
    }

    ///
    bool isRunning();

    ///
    size_t currentLine() const { return currentIndex; }

    ///
    bool isBreakpoint(size_t line) const { return breakpoints.contains(line); }

    ///
    void addBreakpoint(size_t line) { breakpoints.insert(line); }

    ///
    void removeBreakpoint(size_t line) { breakpoints.erase(line); }

    ///
    void toggleBreakpoint(size_t line) {
        if (!isBreakpoint(line)) {
            addBreakpoint(line);
        }
        else {
            removeBreakpoint(line);
        }
    }

private:
    enum class Signal { Sleep, Step, Run };

    /// \Warning requires the calling thread to hold `mutex`
    void send(Signal signal);

    std::thread executionThread;
    std::condition_variable condVar;
    std::mutex mutex;
    Signal signal = Signal::Sleep;
    std::atomic<bool> running = false;
    std::atomic<size_t> currentIndex = 0;

    svm::VirtualMachine vm;
    Disassembly disasm;
    std::array<uint64_t, 2> arguments;
    utl::hashset<size_t> breakpoints;
};

} // namespace sdb

#endif // SDB_PROGRAM_H_
