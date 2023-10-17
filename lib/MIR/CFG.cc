#include "MIR/CFG.h"

#include "IR/CFG/BasicBlock.h"
#include "IR/CFG/Function.h"
#include "MIR/Register.h"

using namespace scatha;
using namespace mir;

Instruction::Instruction(InstCode opcode,
                         Register* dest,
                         utl::small_vector<Value*> operands,
                         std::array<uint64_t, 2> instData,
                         size_t width):
    oc(opcode),
    _width(utl::narrow_cast<uint16_t>(width)),
    _instData{ instData[0], instData[1] } {
    setDest(dest);
    setOperands(std::move(operands));
}

void Instruction::setDest(Register* newDest, size_t numDests) {
    SC_ASSERT(newDest || numDests == 0,
              "If we don't have a dest, then numDests must be 0");
    clearDest();
    _dest = newDest;
    _numDests = utl::narrow_cast<uint16_t>(numDests);
    for (auto* reg: destRegisters()) {
        reg->addDef(this);
    }
}

void Instruction::setDest(Register* dest) { setDest(dest, dest ? 1 : 0); }

void Instruction::setFirstDest(Register* firstDest) {
    auto numDestsTmp = _numDests;
    clearDest();
    _dest = firstDest;
    _numDests = numDestsTmp;
    for (auto* reg: destRegisters()) {
        reg->addDef(this);
    }
}

void Instruction::clearDest() {
    for (auto* reg: destRegisters()) {
        reg->removeDef(this);
    }
    _dest = nullptr;
    _numDests = 0;
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

Register const* Instruction::singleDest() const {
    if (numDests() == 1) {
        return _dest;
    }
    return nullptr;
}

UniquePtr<Instruction> Instruction::clone() {
    return allocate<Instruction>(*this);
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
