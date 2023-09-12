#include "MIR/Register.h"

#include "Common/Ranges.h"
#include "MIR/CFG.h"

using namespace scatha;
using namespace mir;

void Register::addDef(Instruction* inst) {
    visit(*this, [inst](auto& reg) { reg.addDefImpl(inst); });
}

void Register::removeDef(Instruction* inst) {
    visit(*this, [&](auto& reg) { removeDefImpl(inst); });
}

void Register::addUser(Instruction* inst) {
    visit(*this, [&](auto& reg) { addUserImpl(inst); });
}

void Register::removeUser(Instruction* inst) {
    visit(*this, [&](auto& reg) { removeUserImpl(inst); });
}

void Register::replaceWith(Register* repl) {
    auto defs = this->defs() | ToSmallVector<>;
    for (auto* inst: defs) {
        inst->setFirstDest(repl);
    }
    auto uses = this->uses() | ToSmallVector<>;
    for (auto* inst: uses) {
        inst->replaceOperand(this, repl);
    }
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
