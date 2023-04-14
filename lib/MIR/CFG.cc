#include "MIR/CFG.h"

#include "IR/CFG/BasicBlock.h"
#include "IR/CFG/Function.h"
#include "MIR/Register.h"

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
                   size_t numRetvalRegs,
                   Visibility vis):
    ListNodeOverride<Function, Value>(NodeType::Function),
    _name(std::string(irFunc->name())),
    ssaRegs(this),
    virtRegs(this),
    calleeRegs(this),
    hardwareRegs(this),
    irFunc(irFunc),
    numArgRegs(numArgRegs),
    numRetvalRegs(numRetvalRegs),
    vis(vis) {
    for (size_t i = 0; i < numArgRegs; ++i) {
        ssaRegs.add(new SSARegister());
    }
    for (size_t i = 0; i < numRetvalRegs; ++i) {
        virtRegs.add(new VirtualRegister());
    }
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
