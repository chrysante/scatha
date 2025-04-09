#include "MIR/Instructions.h"

#include <utl/functional.hpp>
#include <utl/utility.hpp>

#include "MIR/CFG.h"

using namespace scatha;
using namespace mir;

size_t CallInst::numCalleeOperands() const {
    return isa<CallValueInst>(this) ? 1 : 2;
}

std::span<Value* const> CallInst::arguments() {
    return operands().subspan(numCalleeOperands());
}

std::span<Value const* const> CallInst::arguments() const {
    return operands().subspan(numCalleeOperands());
}

bool CallInst::isNative() const {
    if (auto* cv = dyncast<CallValueInst const*>(this)) {
        return !isa<ForeignFunction>(cv->callee());
    }
    return true;
}

CallInst::CallInst(InstType instType, Register* dest, size_t numDests,
                   utl::small_vector<Value*> operands, Metadata metadata):
    Instruction(instType, dest, numDests, std::move(operands), 0,
                std::move(metadata)),
    numRetRegs(utl::narrow_cast<uint32_t>(numDests)) {}

CallValueInst::CallValueInst(Register* dest, size_t numDests, Value* callee,
                             utl::small_vector<Value*> args, Metadata metadata):
    CallInst(InstType::CallValueInst, dest, numDests,
             (args.insert(args.begin(), callee), std::move(args)),
             std::move(metadata)) {}

CallMemoryInst::CallMemoryInst(Register* dest, size_t numDests,
                               MemoryAddress callee,
                               utl::small_vector<Value*> args,
                               Metadata metadata):
    CallInst(InstType::CallMemoryInst, dest, numDests,
             (args.insert(args.begin(),
                          { callee.baseAddress(), callee.dynOffset() }),
              std::move(args)),
             std::move(metadata)),
    MemoryInst(callee.constantData()) {}

ConversionInst::ConversionInst(Register* dest, Value* operand, Conversion conv,
                               size_t fromBits, size_t toBits,
                               Metadata metadata):
    UnaryInstruction(InstType::ConversionInst, dest, operand,
                     /* byteWidth = */ utl::ceil_divide(fromBits, 8),
                     std::move(metadata)),
    conv(conv),
    _fromBits(utl::narrow_cast<uint16_t>(fromBits)),
    _toBits(utl::narrow_cast<uint16_t>(toBits)) {}

JumpBase::JumpBase(InstType instType, Value* target, Metadata metadata):
    TerminatorInst(instType, { target }, std::move(metadata)) {}

Value const* JumpBase::target() const {
    return cast<Value const*>(operandAt(0));
}
