#include "Model/Breakpoint.h"

#include "Model/Disassembler.h"

using namespace sdb;

void BreakpointManager::add(size_t binaryOffset,
                            std::unique_ptr<Breakpoint> breakpoint) {
    auto itr = breakpoints.find(binaryOffset);
    if (itr == breakpoints.end()) {
        breakpoints.insert({ binaryOffset, std::move(breakpoint) });
        return;
    }
    auto* BP = itr->second.get();
    while (BP->next) {
        BP = BP->next.get();
    }
    BP->next = std::move(breakpoint);
}

void BreakpointManager::addAtInst(size_t index,
                                  std::unique_ptr<Breakpoint> breakpoint) {
    add(disasm->indexToOffset(index), std::move(breakpoint));
}

void BreakpointManager::erase(size_t binaryOffset) {
    eraseIf(binaryOffset, [](Breakpoint const& BP) { return true; });
}

void BreakpointManager::eraseAtInst(size_t index) {
    eraseIf(disasm->indexToOffset(index),
            [](Breakpoint const& BP) { return true; });
}

void BreakpointManager::clear() { breakpoints.clear(); }

Breakpoint const* BreakpointManager::at(size_t offset) const {
    auto itr = breakpoints.find(offset);
    if (itr != breakpoints.end()) {
        return itr->second.get();
    }
    return nullptr;
}

void BreakpointManager::eraseIf(size_t offset, auto cond) {
    auto itr = breakpoints.find(offset);
    if (itr == breakpoints.end()) {
        return;
    }
    Breakpoint* head = itr->second.get();
    Breakpoint* BP = head;
    Breakpoint* prev = nullptr;
    while (true) {
        if (cond(*BP)) {
            break;
        }
        prev = BP;
        BP = BP->next.get();
        if (!BP) {
            return;
        }
    }
    if (prev) {
        prev->next = std::move(BP->next);
        return;
    }
    if (!BP->next) {
        breakpoints.erase(itr);
        return;
    }
    itr->second = std::move(BP->next);
}
