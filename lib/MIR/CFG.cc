#include "MIR/CFG.h"

#include "IR/CFG/BasicBlock.h"
#include "IR/CFG/Function.h"
#include "MIR/Register.h"

using namespace scatha;
using namespace mir;

BasicBlock::BasicBlock(std::string name, ir::BasicBlock const* irBB):
    ListNodeOverride<BasicBlock, Value>(NodeType::BasicBlock),
    ProgramPoint(ProgramPoint::Kind_BasicBlock),
    _name(std::move(name)),
    irBB(irBB) {}

BasicBlock::BasicBlock(ir::BasicBlock const* irBB):
    BasicBlock(std::string(irBB->name()), irBB) {}

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

void Function::linearize() {
    progPoints.clear();
    int index = 0;
    for (auto& BB: *this) {
        BB.ppIdx = index++;
        progPoints.push_back(&BB);
        for (auto& inst: BB) {
            inst.ppIdx = index++;
            progPoints.push_back(&inst);
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
