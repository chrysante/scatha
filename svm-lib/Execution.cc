#include <svm/VirtualMachine.h>

#include <svm/OpCode.h>
#include "Memory.h"

using namespace svm;

namespace {

struct OpcodeOffset {
    OpCode opcode: 12;
    u16 offset: 4;
};

} // namespace

void VirtualMachine::execute(size_t start, std::span<u64 const> arguments) {
    auto const lastframe = execFrames.top() = frame;
    /// We add `MaxCallframeRegisterCount` to the stack pointer because we have
    /// no way of knowing how many registers the currently running execution
    /// frame uses, so we have to assume the worst.
    frame = execFrames.push(ExecutionFrame{
        .regPtr    = lastframe.regPtr + MaxCallframeRegisterCount,
        .bottomReg = lastframe.regPtr + MaxCallframeRegisterCount,
        .iptr      = text.data() + start,
        .stackPtr  = lastframe.stackPtr });
    std::memcpy(frame.regPtr, arguments.data(), arguments.size() * sizeof(u64));
    /// The main loop of the execution
    while (frame.iptr < programBreak) {
        auto [opcode, offset] = load<OpcodeOffset>(frame.iptr);
        assert(utl::to_underlying(opcode) < utl::to_underlying(OpCode::_count) &&
               "Invalid op-code");
        auto const instruction = instructionTable[utl::to_underlying(opcode)];
        instruction(frame.iptr + sizeof(OpCode), frame.regPtr, this);
        frame.iptr += offset;
        ++stats.executedInstructions;
    }
    assert(frame.iptr == programBreak);
    execFrames.pop();
    frame = lastframe;
}
