#include "MIR/Instruction.h"

#include "MIR/CFG.h"
#include "MIR/Register.h"

#include <utl/utility.hpp>

using namespace scatha;
using namespace mir;

Instruction::Instruction(InstType instType, Register* dest, size_t numDests,
                         utl::small_vector<Value*> operands, size_t byteWidth,
                         std::unique_ptr<Metadata> metadata):
    ProgramPoint(ProgramPoint::Kind_Instruction),
    ObjectWithMetadata(std::move(metadata)),
    _instType(instType),
    _byteWidth(utl::narrow_cast<uint8_t>(byteWidth)) {
    setDest(dest, numDests);
    setOperands(std::move(operands));
}

void Instruction::setDest(Register* newDest, size_t numDests) {
    if (!newDest) {
        SC_ASSERT(numDests <= 1, "");
        numDests = 0;
    }
    clearDest();
    _dest = newDest;
    _numDests = utl::narrow_cast<uint8_t>(numDests);
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
    _ops = std::move(operands);
}

void Instruction::setOperandAt(size_t index, Value* newOp) {
    auto* old = _ops[index];
    if (old && isa<Register>(old)) {
        cast<Register*>(old)->removeUser(this);
    }
    _ops[index] = newOp;
    if (auto* reg = dyncast<Register*>(newOp)) {
        reg->addUser(this);
    }
}

void Instruction::clearOperands() {
    for (auto* op: _ops) {
        if (!op) {
            continue;
        }
        if (auto* reg = dyncast<Register*>(op)) {
            reg->removeUser(this);
        }
    }
    _ops.clear();
}

void Instruction::replaceOperand(Value* old, Value* repl) {
    for (auto& op: _ops) {
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
