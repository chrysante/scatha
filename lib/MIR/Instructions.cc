#include "MIR/Instructions.h"

#include <utl/functional.hpp>
#include <utl/utility.hpp>

#include "MIR/CFG.h"

using namespace scatha;
using namespace mir;

ConversionInst::ConversionInst(Register* dest,
                               Value* operand,
                               Conversion conv,
                               size_t fromBits,
                               size_t toBits,
                               Metadata metadata):
    Instruction(InstType::ConversionInst,
                dest,
                1,
                { operand },
                /* byteWidth = */ utl::ceil_divide(fromBits, 8),
                std::move(metadata)),
    conv(conv),
    _fromBits(utl::narrow_cast<uint16_t>(fromBits)),
    _toBits(utl::narrow_cast<uint16_t>(toBits)) {}

JumpBase::JumpBase(InstType instType, BasicBlock* target, Metadata metadata):
    TerminatorInst(instType, { target }, std::move(metadata)) {}

BasicBlock const* JumpBase::target() const {
    return cast<BasicBlock const*>(operandAt(0));
}
