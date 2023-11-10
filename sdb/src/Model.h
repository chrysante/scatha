#ifndef SDB_PROGRAM_H_
#define SDB_PROGRAM_H_

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
    explicit Model(svm::VirtualMachine vm, uint8_t const* program);

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
    bool running() const { return vm.running(); }

    ///
    std::optional<size_t> currentLine() const {
        return disasm.instIndexAt(vm.instructionPointerOffset());
    }

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
    std::thread executionThread;
    svm::VirtualMachine vm;
    Disassembly disasm;
    utl::hashset<size_t> breakpoints;
};

} // namespace sdb

#endif // SDB_PROGRAM_H_
