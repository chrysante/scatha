#include "MIR/CFG.h"

#include "IR/CFG/BasicBlock.h"
#include "IR/CFG/Function.h"

using namespace scatha;
using namespace mir;

Instruction::Instruction(InstCode opcode,
                         Register* dest,
                         utl::small_vector<Value*> operands,
                         uint64_t instData):
    oc(opcode), _dest(nullptr), _instData(instData) {
    setDest(dest);
    setOperands(std::move(operands));
}

void Instruction::setDest(Register* newDest) {
    if (_dest) {
        _dest->removeDef(this);
    }
    if (newDest) {
        newDest->addDef(this);
    }
    _dest = newDest;
}

void Instruction::setOperands(utl::small_vector<Value*> operands) {
    clearOperands();
    for (auto* op: operands) {
        if (!op) {
            continue;
        }
        if (auto* reg = dyncast<Register*>(op)) {
            reg->addUser(this);
        }
    }
    ops = std::move(operands);
}

void Instruction::clearOperands() {
    for (auto* op: ops) {
        if (!op) {
            continue;
        }
        if (auto* reg = dyncast<Register*>(op)) {
            reg->removeUser(this);
        }
    }
    ops.clear();
}

void Register::addDef(Instruction* inst) { _defs.insert(inst); }

void Register::removeDef(Instruction* inst) {
    size_t removeCount = _defs.erase(inst);
    SC_ASSERT(removeCount == 1, "inst was not a definition of this register");
}

void Register::addUser(Instruction* inst) { ++_users[inst]; }

void Register::removeUser(Instruction* inst) {
    auto itr = _users.find(inst);
    SC_ASSERT(itr != _users.end(), "inst is not a user of this register");
    if (--itr->second == 0) {
        _users.erase(itr);
    }
}

BasicBlock::BasicBlock(ir::BasicBlock const* irBB):
    Value(NodeType::BasicBlock), _name(std::string(irBB->name())), irBB(irBB) {}

bool BasicBlock::isEntry() const { return parent()->entry() == this; }

void BasicBlock::insertCallback(Instruction& inst) { inst.set_parent(this); }

void BasicBlock::eraseCallback(Instruction const& inst) {
    auto& mutInst = const_cast<Instruction&>(inst);
    mutInst.clearOperands();
    mutInst.setDest(nullptr);
}

void BasicBlock::addLiveImpl(utl::hashset<Register*>& set,
                             Register* reg,
                             size_t count) {
    for (size_t i = 0; i < count; ++i, reg = reg->next()) {
        set.insert(reg);
    }
}
void BasicBlock::removeLiveImpl(utl::hashset<Register*>& set,
                                Register* reg,
                                size_t count) {
    for (size_t i = 0; i < count; ++i, reg = reg->next()) {
        auto itr = set.find(reg);
        if (itr != set.end()) {
            set.erase(itr);
        }
    }
}

Function::Function(ir::Function const* irFunc):
    Value(NodeType::Function),
    _name(std::string(irFunc->name())),
    irFunc(irFunc) {}

void Function::addRegister(Register* reg) {
    reg->set_parent(this);
    regs.push_back(reg);
}

void Function::insertCallback(BasicBlock& bb) {
    bb.set_parent(this);
    for (auto& inst: bb) {
        bb.insertCallback(inst);
    }
}

void Function::eraseCallback(BasicBlock const& bb) {
    for (auto& inst: bb) {
        const_cast<BasicBlock&>(bb).eraseCallback(inst);
    }
}
