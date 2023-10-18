#include <svm/VirtualMachine.h>

#include <bit>
#include <iostream>

#include <utl/utility.hpp>

#include "svm/BuiltinInternal.h"
#include "svm/Memory.h"
#include "svm/ProgramInternal.h"
#include "svm/VMImpl.h"

using namespace svm;

VirtualMachine::VirtualMachine():
    VirtualMachine(DefaultRegisterCount, DefaultStackSize) {}

VirtualMachine::VirtualMachine(size_t numRegisters, size_t stackSize) {
    impl = std::make_unique<VMImpl>();
    impl->parent = this;
    impl->registers.resize(numRegisters, utl::no_init);
    impl->stackSize = stackSize;
    setFunctionTableSlot(BuiltinFunctionSlot, makeBuiltinTable());
    impl->currentFrame = impl->execFrames.push(
        { .regPtr = impl->registers.data() - MaxCallframeRegisterCount,
          .bottomReg = impl->registers.data() - MaxCallframeRegisterCount,
          .iptr = nullptr,
          .stackPtr = {} });
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
    size_t binSize = utl::round_up(program.binary.size(), 16);
    impl->memory.resizeStaticSlot(binSize + impl->stackSize);
    VirtualPointer staticData{ 0, 0 };
    u8* rawStaticData = &impl->memory.derefAs<u8>(staticData, 0);
    assert(reinterpret_cast<uintptr_t>(rawStaticData) % 16 == 0 &&
           "We just hope this is correctly aligned, if not we'll have to "
           "figure something out");
    std::memcpy(rawStaticData, program.binary.data(), program.binary.size());

    impl->binary = rawStaticData;
    impl->programBreak = impl->binary + program.binary.size();
    impl->startAddress = program.startAddress;
    impl->stackPtr = rawStaticData + binSize;
    impl->currentFrame.stackPtr = staticData + binSize;
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

std::span<u8 const> VirtualMachine::stackData() const {
    return std::span(impl->stackPtr, impl->stackSize);
}

void VirtualMachine::printRegisters(size_t n) const {
    for (size_t i = 0; i < n; ++i) {
        std::cout << "%" << i << ": " << std::hex
                  << impl->currentFrame.regPtr[i] << std::endl;
    }
}

uint64_t VirtualMachine::allocateStackMemory(size_t numBytes) {
    /// TODO: Align the memory to something
    auto result = impl->currentFrame.stackPtr;
    impl->currentFrame.stackPtr += numBytes;
    return std::bit_cast<uint64_t>(result);
}

uint64_t VirtualMachine::allocateMemory(size_t size, size_t align) {
    return std::bit_cast<uint64_t>(impl->memory.allocate(size, align));
}

void VirtualMachine::deallocateMemory(uint64_t ptr, size_t size, size_t align) {
    impl->memory.deallocate(std::bit_cast<VirtualPointer>(ptr), size, align);
}

void* VirtualMachine::derefPointer(uint64_t ptr, size_t numBytes) const {
    return impl->memory.dereference(std::bit_cast<VirtualPointer>(ptr),
                                    numBytes);
}
