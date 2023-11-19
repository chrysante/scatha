#include "Model/Breakpoint.h"

#include "Model/Disassembler.h"

using namespace sdb;

void BreakpointSet::toggle(size_t instIndex) {
    size_t offset = disasm->indexToOffset(instIndex);
    auto itr = set.find(offset);
    if (itr != set.end()) {
        set.erase(itr);
    }
    else {
        set.insert(offset);
    }
}

void BreakpointSet::erase(size_t instIndex) {
    size_t offset = disasm->indexToOffset(instIndex);
    set.erase(offset);
}

void BreakpointSet::clear() { set.clear(); }

bool BreakpointSet::at(size_t instIndex) const {
    size_t offset = disasm->indexToOffset(instIndex);
    return atOffset(offset);
}

bool BreakpointSet::atOffset(size_t offset) const {
    return set.contains(offset);
}
