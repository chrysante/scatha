#include "Model/BreakpointPatcher.h"

using namespace sdb;

void BreakpointPatcher::pushBreakpoint(scdis::InstructionPointerOffset ipo,
                                       bool value) {
    stackMap[ipo].push(value);
    if (value)
        addBreakpoint(ipo);
    else
        removeBreakpoint(ipo);
}

void BreakpointPatcher::popBreakpoint(scdis::InstructionPointerOffset ipo) {
    auto itr = stackMap.find(ipo);
    assert(itr != stackMap.end() && "Have no breakpoint pushed");
    auto& stack = itr->second;
    stack.pop();
    bool haveBreakpoint = !stack.empty() && stack.top();
    if (haveBreakpoint)
        addBreakpoint(ipo);
    else
        removeBreakpoint(ipo);
    if (stack.empty()) stackMap.erase(itr);
}

void BreakpointPatcher::removeAll() {
    insertQueue.clear();
    for (auto& [ipo, stack]: stackMap)
        if (stack.top()) removalQueue.insert(ipo);
    stackMap.clear();
}

void BreakpointPatcher::patchInstructionStream(uint8_t* bin) {
    auto store = [=](scdis::InstructionPointerOffset ipo, uint8_t value) {
        bin[ipo.value] = value;
    };
    for (auto ipo: insertQueue)
        store(ipo, (uint8_t)scbinutil::OpCode::trap);
    for (auto ipo: removalQueue)
        store(ipo, binary[ipo.value]);
    insertQueue.clear();
    removalQueue.clear();
}

void BreakpointPatcher::addBreakpoint(scdis::InstructionPointerOffset ipo) {
    insertQueue.insert(ipo);
    removalQueue.erase(ipo);
}

void BreakpointPatcher::removeBreakpoint(scdis::InstructionPointerOffset ipo) {
    insertQueue.erase(ipo);
    removalQueue.insert(ipo);
}

void BreakpointPatcher::setProgramData(std::span<uint8_t const> progData) {
    scbinutil::ProgramView program(progData.data());
    binary.assign(program.binary.begin(), program.binary.end());
}
