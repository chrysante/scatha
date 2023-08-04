#include <svm/VirtualMachine.h>

#include <iostream>

#include "svm/BuiltinInternal.h"
#include "svm/Memory.h"
#include "svm/ProgramInternal.h"

using namespace svm;

VirtualMachine::VirtualMachine():
    VirtualMachine(DefaultRegisterCount, DefaultStackSize) {}

VirtualMachine::VirtualMachine(size_t numRegisters, size_t stackSize) {
    registers.resize(numRegisters, utl::no_init);
    stack.resize(stackSize, utl::no_init);
    setFunctionTableSlot(BuiltinFunctionSlot, makeBuiltinTable());
}

void VirtualMachine::loadBinary(u8 const* progData) {
    Program program(progData);
    text = std::move(program.instructions);
    data = std::move(program.data);
    startAddress = program.startAddress;
    programBreak = text.data() + text.size();
    frame = execFrames.push(
        { .regPtr = registers.data() - MaxCallframeRegisterCount,
          .bottomReg = registers.data() - MaxCallframeRegisterCount,
          .iptr = nullptr,
          .stackPtr = stack.data() });
}

u64 const* VirtualMachine::execute(std::span<u64 const> arguments) {
    return execute(startAddress, arguments);
}

void VirtualMachine::setFunctionTableSlot(
    size_t slot, utl::vector<ExternalFunction> functions) {
    if (slot >= extFunctionTable.size()) {
        extFunctionTable.resize(slot + 1);
    }
    extFunctionTable[slot] = std::move(functions);
}

void VirtualMachine::setFunction(size_t slot,
                                 size_t index,
                                 ExternalFunction function) {
    if (slot >= extFunctionTable.size()) {
        extFunctionTable.resize(slot + 1);
    }
    auto& slotArray = extFunctionTable[slot];
    if (index >= slotArray.size()) {
        slotArray.resize(index + 1);
    }
    slotArray[index] = function;
}

void VirtualMachine::printRegisters(size_t n) const {
    for (size_t i = 0; i < n; ++i) {
        std::cout << "%" << i << ": " << std::hex << frame.regPtr[i]
                  << std::endl;
    }
}
