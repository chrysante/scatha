#include "MIR/Register.h"

#include "MIR/CFG.h"

using namespace scatha;
using namespace mir;

void Register::addDef(Instruction* inst) {
    visit(*this, [&](auto& reg) { addDefImpl(inst); });
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

void Register::addUserImpl(Instruction* inst) { ++_users[inst]; }

void Register::removeUserImpl(Instruction* inst) {
    auto itr = _users.find(inst);
    SC_ASSERT(itr != _users.end(), "`inst` is not a user of this register");
    if (--itr->second == 0) {
        _users.erase(itr);
    }
}

void SSARegister::addDefImpl(Instruction* inst) {
    SC_ASSERT(!_def, "SSA register can only be assigned once");
    _def = inst;
}

void SSARegister::removeDefImpl(Instruction* inst) {
    SC_ASSERT(_def == inst, "`inst` was not a definition of this register");
    _def = nullptr;
}

void VirtualRegister::addDefImpl(Instruction* inst) { _defs.insert(inst); }

void VirtualRegister::removeDefImpl(Instruction* inst) {
    size_t removeCount = _defs.erase(inst);
    SC_ASSERT(removeCount == 1, "`inst` was not a definition of this register");
}
