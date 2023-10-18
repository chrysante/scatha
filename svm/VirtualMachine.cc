#include <svm/VirtualMachine.h>

#include <iostream>

#include "svm/BuiltinInternal.h"
#include "svm/Memory.h"
#include "svm/ProgramInternal.h"
#include "svm/VMImpl.h"

using namespace svm;

VirtualMachine::VirtualMachine():
    VirtualMachine(DefaultRegisterCount, DefaultStackSize) {}

VirtualMachine::VirtualMachine(size_t numRegisters, size_t stackSize) {
    impl = std::make_unique<VMImpl>();
    impl->registers.resize(numRegisters, utl::no_init);
    impl->stack.resize(stackSize, utl::no_init);
    setFunctionTableSlot(BuiltinFunctionSlot, makeBuiltinTable());
    impl->currentFrame = impl->execFrames.push(
        { .regPtr = impl->registers.data() - MaxCallframeRegisterCount,
          .bottomReg = impl->registers.data() - MaxCallframeRegisterCount,
          .iptr = nullptr,
          .stackPtr = impl->stack.data() });
}

VirtualMachine::VirtualMachine(VirtualMachine&& rhs) noexcept:
    impl(std::move(rhs.impl)) {
    impl->parent = this;
}

VirtualMachine& VirtualMachine::operator=(VirtualMachine&& rhs) noexcept {
    impl = std::move(rhs.impl);
    impl->parent = this;
    return *this;
}

VirtualMachine::~VirtualMachine() = default;

void VirtualMachine::loadBinary(u8 const* progData) {
    ProgramView program(progData);
    impl->binary.assign(program.binary.begin(), program.binary.end());
    assert(reinterpret_cast<uintptr_t>(impl->binary.data()) % 16 == 0 &&
           "We just hope this is correctly aligned, if not we'll have to "
           "figure something out");
    impl->text = impl->binary.data() + program.data.size();
    impl->programBreak = impl->text + program.text.size();
    impl->startAddress = program.startAddress;
}

u8* VirtualMachine::allocateStackMemory(size_t numBytes) {
    /// TODO: Align the memory to something
    auto* result = impl->currentFrame.stackPtr;
    impl->currentFrame.stackPtr += numBytes;
    return result;
}

u64 const* VirtualMachine::execute(std::span<u64 const> arguments) {
    return execute(impl->startAddress, arguments);
}

u64 const* VirtualMachine::execute(size_t startAddress,
                                   std::span<u64 const> arguments) {
    return impl->execute(startAddress, arguments);
}

void VirtualMachine::setFunctionTableSlot(
    size_t slot, std::vector<ExternalFunction> functions) {
    if (slot >= impl->extFunctionTable.size()) {
        impl->extFunctionTable.resize(slot + 1);
    }
    impl->extFunctionTable[slot] = std::move(functions);
}

void VirtualMachine::setFunction(size_t slot,
                                 size_t index,
                                 ExternalFunction function) {
    if (slot >= impl->extFunctionTable.size()) {
        impl->extFunctionTable.resize(slot + 1);
    }
    auto& slotArray = impl->extFunctionTable[slot];
    if (index >= slotArray.size()) {
        slotArray.resize(index + 1);
    }
    slotArray[index] = function;
}

std::span<u64 const> VirtualMachine::registerData() const {
    return impl->registers;
}

u64 VirtualMachine::getRegister(size_t index) const {
    return impl->registers[index];
}

std::span<u8 const> VirtualMachine::stackData() const { return impl->stack; }

void VirtualMachine::printRegisters(size_t n) const {
    for (size_t i = 0; i < n; ++i) {
        std::cout << "%" << i << ": " << std::hex
                  << impl->currentFrame.regPtr[i] << std::endl;
    }
}
