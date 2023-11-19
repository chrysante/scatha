#include <svm/VirtualMachine.h>

#include <bit>
#include <iostream>

#include <utl/utility.hpp>

#include "svm/BuiltinInternal.h"
#include "svm/Memory.h"
#include "svm/Program.h"
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
    auto staticData = VirtualMemory::MakeStaticDataPointer(0);
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

void VirtualMachine::beginExecution(std::span<u64 const> arguments) {
    return impl->beginExecution(impl->startAddress, arguments);
}

void VirtualMachine::beginExecution(size_t startAddress,
                                    std::span<u64 const> arguments) {
    return impl->beginExecution(startAddress, arguments);
}

bool VirtualMachine::running() const { return impl->running(); }

void VirtualMachine::stepExecution() { impl->stepExecution(); }

u64 const* VirtualMachine::endExecution() { return impl->endExecution(); }

size_t VirtualMachine::instructionPointerOffset() const {
    return impl->instructionPointerOffset();
}

void VirtualMachine::setInstructionPointerOffset(size_t offset) {
    impl->setInstructionPointerOffset(offset);
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

CompareFlags VirtualMachine::getCompareFlags() const { return impl->cmpFlags; }

ExecutionFrame VirtualMachine::getCurrentExecFrame() const {
    return impl->currentFrame;
}

void VirtualMachine::printRegisters(size_t n) const {
    for (size_t i = 0; i < n; ++i) {
        std::cout << "%" << i << ": " << std::hex
                  << impl->currentFrame.regPtr[i] << std::endl;
    }
}

VirtualPointer VirtualMachine::allocateStackMemory(size_t numBytes,
                                                   size_t align) {
    alignTo(impl->currentFrame.stackPtr, align);
    auto result = impl->currentFrame.stackPtr;
    impl->currentFrame.stackPtr += numBytes;
    alignTo(impl->currentFrame.stackPtr, 8);
    return result;
}

VirtualPointer VirtualMachine::allocateMemory(size_t size, size_t align) {
    return impl->memory.allocate(size, align);
}

void VirtualMachine::deallocateMemory(VirtualPointer ptr,
                                      size_t size,
                                      size_t align) {
    impl->memory.deallocate(ptr, size, align);
}

ptrdiff_t VirtualMachine::validPtrRange(VirtualPointer ptr) const {
    return impl->memory.validRange(ptr);
}

void* VirtualMachine::derefPointer(VirtualPointer ptr, size_t numBytes) const {
    return impl->memory.dereference(ptr, numBytes);
}

void VirtualMachine::setIOStreams(std::istream* in, std::ostream* out) {
    if (in) {
        impl->istream = in;
    }
    if (out) {
        impl->ostream = out;
    }
}

std::istream& VirtualMachine::istream() const { return *impl->istream; }

std::ostream& VirtualMachine::ostream() const { return *impl->ostream; }

VMImpl::VMImpl(): istream(&std::cin), ostream(&std::cout) {}
