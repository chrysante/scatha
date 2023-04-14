#include <svm/VirtualMachine.h>

#include <svm/OpCode.h>

using namespace svm;

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
        OpCode const opcode = OpCode(*frame.iptr);
        assert(static_cast<u8>(opcode) < static_cast<u8>(OpCode::_count) &&
               "Invalid op-code");
        auto const instruction = instructionTable[static_cast<u8>(opcode)];
        u64 const offset =
            instruction(frame.iptr + sizeof(OpCode), frame.regPtr, this);
        frame.iptr += offset;
        ++stats.executedInstructions;
    }
    assert(frame.iptr == programBreak);
    execFrames.pop();
    frame = lastframe;
}
