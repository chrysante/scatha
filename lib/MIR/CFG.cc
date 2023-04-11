#include "MIR/CFG.h"

#include "IR/CFG/BasicBlock.h"
#include "IR/CFG/Function.h"

using namespace scatha;
using namespace mir;

Instruction::Instruction(InstCode opcode,
                         Register* dest,
                         utl::small_vector<Value*> operands,
                         uint64_t instData,
                         size_t width):
    oc(opcode),
    _width(utl::narrow_cast<uint16_t>(width)),
    _dest(nullptr),
    _instData(instData) {
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

void Instruction::setOperandAt(size_t index, Value* newOp) {
    auto* old = ops[index];
    if (old && isa<Register>(old)) {
        cast<Register*>(old)->removeUser(this);
    }
    ops[index] = newOp;
    if (auto* reg = dyncast<Register*>(newOp)) {
        reg->addUser(this);
    }
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

void Instruction::replaceOperand(Value* old, Value* repl) {
    for (auto& op: ops) {
        if (op != old) {
            continue;
        }
        if (auto* oldReg = dyncast<Register*>(old)) {
            oldReg->removeUser(this);
        }
        if (auto* replReg = dyncast<Register*>(repl)) {
            replReg->addUser(this);
        }
        op = repl;
    }
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
    ListNodeOverride<BasicBlock, Value>(NodeType::BasicBlock),
    _name(std::string(irBB->name())),
    irBB(irBB) {}

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

Function::Function(ir::Function const* irFunc,
                   size_t numArgRegs,
                   size_t numRetvalRegs):
    ListNodeOverride<Function, Value>(NodeType::Function),
    _name(std::string(irFunc->name())),
    irFunc(irFunc),
    numArgRegs(numArgRegs),
    numRetvalRegs(numRetvalRegs) {
    size_t const numRegs = std::max(numArgRegs, numRetvalRegs);
    for (size_t i = 0; i < numRegs; ++i) {
        addVirtualRegister();
    }
}

Register* Function::addVirtualRegister() {
    Register* reg = new Register();
    reg->setIndex(flatRegs.size());
    reg->set_parent(this);
    virtRegs.push_back(reg);
    flatRegs.push_back(reg);
    return reg;
}

void Function::eraseVirtualRegister(Register* reg) {
    flatRegs[reg->index()] = nullptr;
    virtRegs.erase(reg);
}

void Function::renameRegisters() {
    auto itr = ranges::remove(flatRegs, nullptr);
    flatRegs.erase(itr, flatRegs.end());
    for (auto [index, reg]: flatRegs | ranges::views::enumerate) {
        reg->setIndex(index);
    }
}

void Function::addCalleeRegister(Register* reg) {
    if (calleeRegs.empty()) {
        reg->setIndex(0);
    }
    else {
        reg->setIndex(calleeRegs.back().index() + 1);
    }
    reg->set_parent(this);
    calleeRegs.push_back(reg);
}

void Function::linearizeInstructions() {
    instrs.clear();
    size_t index = 0;
    for (auto& BB: *this) {
        for (auto& inst: BB) {
            inst.setIndex(index++);
            instrs.push_back(&inst);
        }
    }
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
