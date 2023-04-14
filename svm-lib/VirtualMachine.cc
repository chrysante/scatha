#include <svm/VirtualMachine.h>

#include <svm/OpCode.h> // Delete once instruction table is gone.

#include <svm/Builtin.h>
#include <utl/math.hpp>
#include <utl/utility.hpp>

#include "Memory.h"
#include "ProgramInternal.h"

using namespace svm;

VirtualMachine::VirtualMachine():
    VirtualMachine(DefaultRegisterCount, DefaultStackSize) {}

VirtualMachine::VirtualMachine(size_t numRegisters, size_t stackSize):
    instructionTable(makeInstructionTable()) {
    registers.resize(numRegisters, utl::no_init);
    stack.resize(stackSize, utl::no_init);
    setFunctionTableSlot(builtinFunctionSlot, makeBuiltinTable());
}

void VirtualMachine::loadProgram(u8 const* progData) {
    Program program(progData);
    text         = std::move(program.instructions);
    data         = std::move(program.data);
    startAddress = program.startAddress;
    programBreak = text.data() + text.size();
    frame        = execFrames.push(
        { .regPtr    = registers.data() - MaxCallframeRegisterCount,
                 .bottomReg = registers.data() - MaxCallframeRegisterCount,
                 .iptr      = nullptr,
                 .stackPtr  = stack.data() });
}

void VirtualMachine::execute(std::span<u64 const> arguments) {
    execute(startAddress, arguments);
}

void VirtualMachine::setFunctionTableSlot(
    size_t slot, utl::vector<ExternalFunction> functions) {
    if (slot >= extFunctionTable.size()) {
        extFunctionTable.resize(slot + 1);
    }
    extFunctionTable[slot] = std::move(functions);
}
