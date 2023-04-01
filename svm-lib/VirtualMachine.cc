#include <svm/VirtualMachine.h>

#include <iostream>

#include <svm/Builtin.h>
#include <svm/OpCode.h>
#include <utl/math.hpp>
#include <utl/utility.hpp>

#include "Memory.h"
#include "ProgramInternal.h"

using namespace svm;

VirtualMachine::VirtualMachine(): instructionTable(makeInstructionTable()) {
    setFunctionTableSlot(builtinFunctionSlot, makeBuiltinTable());
}

void VirtualMachine::loadProgram(u8 const* progData) {
    Program program(progData);
    instructionCount = program.instructions.size();
    text = std::move(program.instructions);
    data = std::move(program.data);
    programStart = program.start;
    iptr         = text.data() + programStart;
    programBreak = text.data() + text.size();
}

void VirtualMachine::execute() {
    registers.resize(defaultRegisterCount);
    regPtr = registers.data();
    stack.resize(defaultStackSize);
    stackPtr = stack.data();
    while (iptr < programBreak) {
        OpCode const opCode{ *iptr };
        assert(static_cast<u8>(opCode) < static_cast<u8>(OpCode::_count) &&
               "Invalid op-code");
        auto const instruction = instructionTable[static_cast<u8>(opCode)];
        u64 const offset       = instruction(iptr + 1, regPtr, this);
        iptr += offset;
        ++stats.executedInstructions;
    }
    assert(iptr == programBreak);
    cleanup();
}

void VirtualMachine::addExternalFunction(size_t slot, ExternalFunction f) {
    if (slot >= extFunctionTable.size()) {
        extFunctionTable.resize(slot + 1);
    }
    extFunctionTable[slot].push_back(f);
}

void VirtualMachine::setFunctionTableSlot(
    size_t slot, utl::vector<ExternalFunction> functions) {
    if (slot >= extFunctionTable.size()) {
        extFunctionTable.resize(slot + 1);
    }
    extFunctionTable[slot] = std::move(functions);
}

void VirtualMachine::cleanup() {
    iptr = text.data() + programStart;
}

size_t VirtualMachine::defaultRegisterCount = 1 << 20;

size_t VirtualMachine::defaultStackSize = 1 << 20;
