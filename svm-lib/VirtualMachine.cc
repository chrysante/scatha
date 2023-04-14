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
    registers.resize(defaultRegisterCount);
    stack.resize(defaultStackSize);
    instructionCount = program.instructions.size();
    text             = std::move(program.instructions);
    data             = std::move(program.data);
    startAddress     = program.startAddress;
    programBreak     = text.data() + text.size();
    ctx              = execContexts.push({ .regPtr    = registers.data() - 256,
                                           .bottomReg = registers.data() - 256,
                                           .iptr      = nullptr,
                                           .stackPtr  = stack.data() });
}

void VirtualMachine::execute(std::span<u64 const> arguments) { execute(startAddress, arguments); }

void VirtualMachine::execute(size_t start, std::span<u64 const> arguments) {
    auto const lastCtx = execContexts.top() = ctx;
    ctx = execContexts.push(ExecutionContext{ .regPtr    = lastCtx.regPtr + 256,
                                              .bottomReg = lastCtx.regPtr + 256,
                                              .iptr      = text.data() + start,
                                              .stackPtr  = lastCtx.stackPtr });
    std::memcpy(ctx.regPtr, arguments.data(), arguments.size() * sizeof(u64));
    while (ctx.iptr < programBreak) {
        OpCode const opCode{ *ctx.iptr };
        assert(static_cast<u8>(opCode) < static_cast<u8>(OpCode::_count) &&
               "Invalid op-code");
        auto const instruction = instructionTable[static_cast<u8>(opCode)];
        u64 const offset       = instruction(ctx.iptr + 1, ctx.regPtr, this);
        ctx.iptr += offset;
        ++stats.executedInstructions;
    }
    assert(ctx.iptr == programBreak);
    execContexts.pop();
    ctx = lastCtx;
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

size_t VirtualMachine::defaultRegisterCount = 1 << 20;

size_t VirtualMachine::defaultStackSize = 1 << 20;
