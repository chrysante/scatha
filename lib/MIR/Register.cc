#include "MIR/Register.h"

#include <range/v3/algorithm.hpp>

#include "Common/Ranges.h"
#include "MIR/CFG.h"
#include "MIR/Instruction.h"

using namespace scatha;
using namespace mir;

void Register::addDef(Instruction* inst) {
    visit(*this, [inst](auto& reg) { reg.addDefImpl(inst); });
}

void Register::removeDef(Instruction* inst) {
    visit(*this, [&](auto&) { removeDefImpl(inst); });
}

void Register::addUser(Instruction* inst) {
    visit(*this, [&](auto&) { addUserImpl(inst); });
}

void Register::removeUser(Instruction* inst) {
    visit(*this, [&](auto&) { removeUserImpl(inst); });
}

void Register::replaceDefsWith(Register* repl) {
    if (this == repl) {
        return;
    }
    auto defs = this->defs() | ToSmallVector<>;
    for (auto* inst: defs) {
        inst->setFirstDest(repl);
    }
}

void Register::replaceUsesWith(Register* repl) {
    if (this == repl) {
        return;
    }
    auto uses = this->uses() | ToSmallVector<>;
    for (auto* inst: uses) {
        inst->replaceOperand(this, repl);
    }
}

void Register::replaceWith(Register* repl) {
    replaceDefsWith(repl);
    replaceUsesWith(repl);
}

std::optional<LiveInterval> Register::liveIntervalAt(int programPoint) {
    for (auto I: liveRange()) {
        if (compare(I, programPoint) == 0) {
            return I;
        }
    }
    return std::nullopt;
}

void Register::addLiveInterval(LiveInterval I) {
    auto itr = ranges::lower_bound(_liveRange, I);
    _liveRange.insert(itr, I);
}

void Register::removeLiveInterval(LiveInterval I) {
    auto itr = ranges::lower_bound(_liveRange, I);
    SC_EXPECT(itr != _liveRange.end());
    SC_EXPECT(*itr == I);
    _liveRange.erase(itr);
}

void Register::replaceLiveInterval(LiveInterval orig, LiveInterval repl) {
    auto itr = ranges::lower_bound(_liveRange, orig);
    SC_EXPECT(itr != _liveRange.end());
    SC_EXPECT(*itr == orig);
    *itr = repl;
}

void Register::addDefImpl(Instruction* inst) { _defs.insert(inst); }

void Register::removeDefImpl(Instruction* inst) {
    size_t removeCount = _defs.erase(inst);
    SC_ASSERT(removeCount == 1, "`inst` was not a definition of this register");
}

void Register::addUserImpl(Instruction* inst) { ++_users[inst]; }

void Register::removeUserImpl(Instruction* inst) {
    auto itr = _users.find(inst);
    SC_ASSERT(itr != _users.end(), "`inst` is not a user of this register");
    if (--itr->second == 0) {
        _users.erase(itr);
    }
}

void SSARegister::addDefImpl(Instruction* inst) {
    SC_ASSERT(defs().empty(), "SSA register can only be assigned once");
    Register::addDefImpl(inst);
}

CalleeRegister::CalleeRegister():
    Register::Override<CalleeRegister>(NodeType::CalleeRegister) {
    /// Callee registers are always fixed
    setFixed();
}
