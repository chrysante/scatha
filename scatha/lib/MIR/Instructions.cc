#include "MIR/Instructions.h"

#include <utl/functional.hpp>
#include <utl/utility.hpp>

#include "MIR/CFG.h"

using namespace scatha;
using namespace mir;

CallInst::CallInst(Register* dest, size_t numDests, Value* callee,
                   utl::small_vector<Value*> args, Metadata metadata):
    Instruction(InstType::CallInst, dest, numDests,
                (args.insert(args.begin(), callee), std::move(args)), 0,
                std::move(metadata)),
    numRetRegs(utl::narrow_cast<uint32_t>(numDests)) {}

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
