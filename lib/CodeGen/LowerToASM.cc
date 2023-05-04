#include "CodeGen/Passes.h"

#include "Assembly/AssemblyStream.h"
#include "Assembly/Block.h"
#include "Assembly/Instruction.h"
#include "Assembly/Value.h"
#include "MIR/CFG.h"
#include "MIR/Module.h"

using namespace scatha;
using namespace mir;
using namespace Asm;

namespace {

struct CGContext {
    CGContext(Asm::AssemblyStream& result): result(result) {}

    void run(mir::Module const& mod);

    void genFunction(mir::Function const& func);
    void genBlock(mir::BasicBlock const& block);
    void genInst(mir::Instruction const& inst);

    size_t getLabelID(mir::Value const& value) {
        auto [itr, success] =
            labelIndices.insert({ &value, labelIndexCounter });
        if (success) {
            ++labelIndexCounter;
        }
        return itr->second;
    }

    Asm::RegisterIndex toRegIdx(mir::Register const* reg) const {
        SC_ASSERT(
            reg->nodeType() == mir::NodeType::HardwareRegister,
            "At this point we expect all registers to be hardware registers");
        return Asm::RegisterIndex(reg->index());
    }

    Asm::RegisterIndex toRegIdx(mir::Value const* value) const {
        return toRegIdx(cast<mir::Register const*>(value));
    }

    Asm::Value toValue(mir::Value const* value) const {
        // clang-format off
        return visit(*value, utl::overload{
            [](mir::Constant const& constant) -> Asm::Value {
                switch (constant.bytewidth()) {
                case 1: return Value8(constant.value());
                case 2: return Value16(constant.value());
                case 4: return Value32(constant.value());
                case 8: return Value64(constant.value());
                default: SC_UNREACHABLE();
                }
            },
            [](mir::UndefValue const&) -> Asm::Value {
                return Asm::RegisterIndex(0);
            },
            [&](mir::Register const& reg) -> Asm::Value {
                return toRegIdx(&reg);
            },
            [](mir::Value const& value) -> Asm::Value {
                SC_UNREACHABLE();
            }
        }); // clang-format on
    }

    Asm::AssemblyStream& result;
    Asm::Block* currentBlock             = nullptr;
    mir::Function const* currentFunction = nullptr;

    /// Maps basic blocks and functions to label IDs
    utl::hashmap<mir::Value const*, size_t> labelIndices;
    size_t labelIndexCounter = 0;
};

} // namespace

Asm::AssemblyStream cg::lowerToASM(mir::Module const& mod) {
    Asm::AssemblyStream result;
    result.setDataSection(mod.dataSection());
    CGContext ctx(result);
    ctx.run(mod);
    return result;
}

void CGContext::run(mir::Module const& mod) {
    for (auto& F: mod) {
        genFunction(F);
    }
}

void CGContext::genFunction(mir::Function const& F) {
    currentBlock = result.add(Asm::Block(getLabelID(F), std::string(F.name())));
    if (F.visibility() == mir::Visibility::Extern) {
        currentBlock->setPublic();
    }
    currentFunction = &F;
    for (auto& BB: F) {
        genBlock(BB);
    }
}

void CGContext::genBlock(mir::BasicBlock const& BB) {
    if (!BB.isEntry()) {
        currentBlock =
            result.add(Asm::Block(getLabelID(BB), std::string(BB.name())));
    }
    for (auto& inst: BB) {
        genInst(inst);
    }
}

static Asm::MemoryAddress getAddressOperand(mir::Instruction const& inst) {
    auto* base   = cast<mir::Register const*>(inst.operandAt(0));
    auto* factor = cast_or_null<mir::Register const*>(inst.operandAt(1));
    auto data    = inst.instDataAs<mir::MemoryAddress::ConstantData>();
    return Asm::MemoryAddress(RegisterIndex(base->index()),
                              factor ? RegisterIndex(factor->index()) :
                                       Asm::MemoryAddress::InvalidRegisterIndex,
                              data.offsetFactor,
                              data.offsetTerm);
}

static Asm::UnaryArithmeticOperation mapUnaryArithmetic(
    mir::UnaryArithmeticOperation op);

static Asm::ArithmeticOperation mapArithmetic(mir::ArithmeticOperation op);

static Asm::CompareOperation mapCompareOperation(mir::CompareOperation op);

static Asm::Type mapCompareMode(ir::CompareMode mode);

void CGContext::genInst(mir::Instruction const& inst) {
    switch (inst.instcode()) {
    case mir::InstCode::Store: {
        auto dest   = getAddressOperand(inst);
        auto source = toRegIdx(inst.operandAt(2));
        currentBlock->insertBack(MoveInst(dest, source, inst.bytewidth()));
        break;
    }
    case mir::InstCode::Load: {
        auto dest   = toRegIdx(inst.dest());
        auto source = getAddressOperand(inst);
        currentBlock->insertBack(MoveInst(dest, source, inst.bytewidth()));
        break;
    }
    case mir::InstCode::Copy: {
        auto dest   = toRegIdx(inst.dest());
        auto source = toValue(inst.operandAt(0));
        currentBlock->insertBack(MoveInst(dest, source, inst.bytewidth()));
        break;
    }
    case mir::InstCode::Call: {
        auto callData = inst.instDataAs<mir::CallInstData>();
        auto* callee  = cast<mir::Function const*>(inst.operandAt(0));
        currentBlock->insertBack(
            CallInst(getLabelID(*callee), callData.regOffset));
        break;
    }
    case mir::InstCode::CallExt: {
        auto callData = inst.instDataAs<mir::CallInstData>();
        auto callee   = callData.extFuncAddress;
        currentBlock->insertBack(
            CallExtInst(callData.regOffset, callee.slot, callee.index));
        break;
    }
    case mir::InstCode::CondCopy: {
        auto dest   = toRegIdx(inst.dest());
        auto source = toValue(inst.operandAt(0));
        auto cond   = inst.instDataAs<mir::CompareOperation>();
        currentBlock->insertBack(CMoveInst(mapCompareOperation(cond),
                                           dest,
                                           source,
                                           inst.bytewidth()));
        break;
    }
    case mir::InstCode::LIncSP: {
        auto dest     = toRegIdx(inst.dest());
        auto numBytes = toValue(inst.operandAt(0));
        currentBlock->insertBack(
            LIncSPInst(dest, numBytes.get<Asm::Value16>()));
        break;
    }
    case mir::InstCode::LEA: {
        auto dest    = toRegIdx(inst.dest());
        auto address = getAddressOperand(inst);
        currentBlock->insertBack(LEAInst(dest, address));
        break;
    }
    case mir::InstCode::LDA: {
        auto dest   = toRegIdx(inst.dest());
        auto offset = toValue(inst.operandAt(0)).get<Asm::Value32>();
        currentBlock->insertBack(LDAInst(dest, offset));
        break;
    }
    case mir::InstCode::Compare: {
        auto LHS  = toValue(inst.operandAt(0));
        auto RHS  = toValue(inst.operandAt(1));
        auto mode = inst.instDataAs<mir::CompareMode>();
        currentBlock->insertBack(
            Asm::CompareInst(mapCompareMode(mode), LHS, RHS, inst.bytewidth()));
        break;
    }
    case mir::InstCode::Test: {
        auto operand = toValue(inst.operandAt(0));
        auto mode    = inst.instDataAs<mir::CompareMode>();
        currentBlock->insertBack(
            TestInst(mapCompareMode(mode), operand, inst.bytewidth()));
        break;
    }
    case mir::InstCode::Set: {
        auto dest      = toRegIdx(inst.dest());
        auto operation = inst.instDataAs<mir::CompareOperation>();
        currentBlock->insertBack(SetInst(dest, mapCompareOperation(operation)));
        break;
    }
    case mir::InstCode::UnaryArithmetic: {
        auto dest      = toRegIdx(inst.dest());
        auto operand   = toRegIdx(inst.operandAt(0));
        auto operation = inst.instDataAs<mir::UnaryArithmeticOperation>();
        if (inst.dest() != inst.operandAt(0)) {
            currentBlock->insertBack(MoveInst(dest, operand, inst.bytewidth()));
        }
        currentBlock->insertBack(
            UnaryArithmeticInst(mapUnaryArithmetic(operation), dest));
        break;
    }
    case mir::InstCode::Arithmetic: {
        auto dest      = toRegIdx(inst.dest());
        auto LHS       = toRegIdx(inst.operandAt(0));
        auto RHS       = toValue(inst.operandAt(1));
        auto operation = inst.instDataAs<mir::ArithmeticOperation>();
        if (inst.dest() != inst.operandAt(0)) {
            currentBlock->insertBack(MoveInst(dest, LHS, inst.bytewidth()));
        }
        currentBlock->insertBack(Asm::ArithmeticInst(mapArithmetic(operation),
                                                     dest,
                                                     RHS,
                                                     inst.bytewidth()));
        break;
    }
    case mir::InstCode::Conversion: {
        auto dest    = toRegIdx(inst.dest());
        auto operand = toRegIdx(inst.operandAt(0));
        auto conv    = inst.instDataAs<mir::Conversion>();
        auto type    = [&] {
            switch (conv) {
            case mir::Conversion::Sext:
                return Asm::Type::Signed;
            case mir::Conversion::Fext:
                return Asm::Type::Float;
            case mir::Conversion::Ftrunc:
                return Asm::Type::Float;
            default:
                SC_UNREACHABLE();
            }
        }();
        if (inst.dest() != inst.operandAt(0)) {
            currentBlock->insertBack(MoveInst(dest, operand, inst.bytewidth()));
        }
        currentBlock->insertBack(ConvInst(dest, type, inst.bitwidth()));
        break;
    }
    case mir::InstCode::Jump: {
        auto* target = inst.operandAt(0);
        currentBlock->insertBack(JumpInst(getLabelID(*target)));
        break;
    }
    case mir::InstCode::CondJump: {
        auto* target   = inst.operandAt(0);
        auto condition = inst.instDataAs<mir::CompareOperation>();
        currentBlock->insertBack(
            JumpInst(mapCompareOperation(condition), getLabelID(*target)));
        break;
    }
    case mir::InstCode::Return: {
        currentBlock->insertBack(ReturnInst());
        break;
    }
    default:
        SC_UNREACHABLE();
    }
}

static Asm::UnaryArithmeticOperation mapUnaryArithmetic(
    mir::UnaryArithmeticOperation op) {
    // clang-format off
    return UTL_MAP_ENUM(op, Asm::UnaryArithmeticOperation, {
        { mir::UnaryArithmeticOperation::BitwiseNot,   Asm::UnaryArithmeticOperation::BitwiseNot  },
        { mir::UnaryArithmeticOperation::LogicalNot,   Asm::UnaryArithmeticOperation::LogicalNot  },
    }); // clang-format on
}

static Asm::ArithmeticOperation mapArithmetic(mir::ArithmeticOperation op) {
    // clang-format off
    return UTL_MAP_ENUM(op, Asm::ArithmeticOperation, {
        { mir::ArithmeticOperation::Add,   Asm::ArithmeticOperation::Add  },
        { mir::ArithmeticOperation::Sub,   Asm::ArithmeticOperation::Sub  },
        { mir::ArithmeticOperation::Mul,   Asm::ArithmeticOperation::Mul  },
        { mir::ArithmeticOperation::SDiv,  Asm::ArithmeticOperation::SDiv },
        { mir::ArithmeticOperation::UDiv,  Asm::ArithmeticOperation::UDiv },
        { mir::ArithmeticOperation::SRem,  Asm::ArithmeticOperation::SRem },
        { mir::ArithmeticOperation::URem,  Asm::ArithmeticOperation::URem },
        { mir::ArithmeticOperation::FAdd,  Asm::ArithmeticOperation::FAdd },
        { mir::ArithmeticOperation::FSub,  Asm::ArithmeticOperation::FSub },
        { mir::ArithmeticOperation::FMul,  Asm::ArithmeticOperation::FMul },
        { mir::ArithmeticOperation::FDiv,  Asm::ArithmeticOperation::FDiv },
        { mir::ArithmeticOperation::LShL,  Asm::ArithmeticOperation::LShL },
        { mir::ArithmeticOperation::LShR,  Asm::ArithmeticOperation::LShR },
        { mir::ArithmeticOperation::AShL,  Asm::ArithmeticOperation::AShL },
        { mir::ArithmeticOperation::AShR,  Asm::ArithmeticOperation::AShR },
        { mir::ArithmeticOperation::And,   Asm::ArithmeticOperation::And  },
        { mir::ArithmeticOperation::Or,    Asm::ArithmeticOperation::Or   },
        { mir::ArithmeticOperation::XOr,   Asm::ArithmeticOperation::XOr  },
    });
    // clang-format on
}

static Asm::CompareOperation mapCompareOperation(mir::CompareOperation op) {
    // clang-format off
    return UTL_MAP_ENUM(op, Asm::CompareOperation, {
        { mir::CompareOperation::Less,      Asm::CompareOperation::Less      },
        { mir::CompareOperation::LessEq,    Asm::CompareOperation::LessEq    },
        { mir::CompareOperation::Greater,   Asm::CompareOperation::Greater   },
        { mir::CompareOperation::GreaterEq, Asm::CompareOperation::GreaterEq },
        { mir::CompareOperation::Equal,     Asm::CompareOperation::Eq        },
        { mir::CompareOperation::NotEqual,  Asm::CompareOperation::NotEq     },
    });
    // clang-format on
}

static Asm::Type mapCompareMode(ir::CompareMode mode) {
    switch (mode) {
    case ir::CompareMode::Signed:
        return Type::Signed;
    case ir::CompareMode::Unsigned:
        return Type::Unsigned;
    case ir::CompareMode::Float:
        return Type::Float;
    case ir::CompareMode::_count:
        SC_UNREACHABLE();
    }
}
