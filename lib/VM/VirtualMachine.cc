#include "VM/VirtualMachine.h"

#include <iostream>

#include <utl/math.hpp>
#include <utl/utility.hpp>

#include "Basic/Memory.h"
#include "VM/Builtin.h"
#include "VM/OpCode.h"

namespace scatha::vm {

VirtualMachine::VirtualMachine(): instructionTable(makeInstructionTable()) {
    setFunctionTableSlot(builtinFunctionSlot, makeBuiltinTable());
}

void VirtualMachine::load(Program const& program) {
    instructionCount = program.instructions.size();
    // Load the program into memory
    memory.resize(instructionCount);
    std::memcpy(memory.data(), program.instructions.data(), instructionCount);
    iptr         = memory.data() + program.start;
    programBreak = memory.data() + instructionCount;
    memoryPtr    = nullptr;
    memoryBreak  = nullptr;
}

void VirtualMachine::execute() {
    SC_ASSERT(iptr >= memory.data(), "");
    registers.resize(defaultRegisterCount);
    regPtr = registers.data();
    while (iptr < programBreak) {
        OpCode const opCode{ *iptr };
        SC_ASSERT(static_cast<u8>(opCode) < static_cast<u8>(OpCode::_count), "Invalid op-code");
        auto const instruction = instructionTable[static_cast<u8>(opCode)];
        u64 const offset       = instruction(iptr + 1, regPtr, this);
        iptr += offset;
        ++stats.executedInstructions;
    }
    SC_ASSERT(iptr == programBreak, "");
    cleanup();
}

void VirtualMachine::addExternalFunction(size_t slot, ExternalFunction f) {
    if (slot >= extFunctionTable.size()) {
        extFunctionTable.resize(slot + 1);
    }
    extFunctionTable[slot].push_back(f);
}

void VirtualMachine::setFunctionTableSlot(size_t slot, utl::vector<ExternalFunction> functions) {
    if (slot >= extFunctionTable.size()) {
        extFunctionTable.resize(slot + 1);
    }
    extFunctionTable[slot] = std::move(functions);
}

void VirtualMachine::resizeMemory(size_t newSize) {
    size_t const iptrOffset             = memory.empty() ? 0 : utl::narrow_cast<size_t>(iptr - memory.data());
    size_t const programBreakOffset     = utl::narrow_cast<size_t>(programBreak - iptr);
    size_t const memoryBreakOffset      = utl::narrow_cast<size_t>(memoryBreak - memoryPtr);
    size_t const paddedInstructionCount = utl::round_up_pow_two(instructionCount, 16);
    memory.resize(paddedInstructionCount + newSize);
    iptr         = memory.data() + iptrOffset;
    programBreak = iptr + programBreakOffset;
    memoryPtr    = memory.data() + paddedInstructionCount;
    memoryBreak  = memoryPtr + memoryBreakOffset;
}

void VirtualMachine::cleanup() {
    iptr   = memory.data();
    regPtr = registers.data();
}

size_t VirtualMachine::defaultRegisterCount = 1 << 20;

} // namespace scatha::vm
