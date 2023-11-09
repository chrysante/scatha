#ifndef SDB_PROGRAM_H_
#define SDB_PROGRAM_H_

#include <span>
#include <string>
#include <unordered_set>
#include <vector>

namespace sdb {

struct Instruction {
    std::string text;
};

class Program {
public:
    explicit Program(std::vector<Instruction> instructions):
        _insts(std::move(instructions)) {}

    void toggleExecution();

    void skipLine();

    void enterFunction();

    void exitFunction();

    std::span<Instruction const> instructions() const { return _insts; }

    bool running() const { return _running; }

    size_t currentLine() const { return _current; }

    bool isBreakpoint(size_t line) const { return breakpoints.contains(line); }

    void addBreakpoint(size_t line) { breakpoints.insert(line); }

    void removeBreakpoint(size_t line) { breakpoints.erase(line); }

    void toggleBreakpoint(size_t line) {
        if (!isBreakpoint(line)) {
            addBreakpoint(line);
        }
        else {
            removeBreakpoint(line);
        }
    }

private:
    std::vector<Instruction> _insts;
    std::unordered_set<size_t> breakpoints;
    size_t _current = 0;
    bool _running = false;
};

} // namespace sdb

#endif // SDB_PROGRAM_H_
