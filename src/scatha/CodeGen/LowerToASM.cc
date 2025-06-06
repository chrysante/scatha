#include "CodeGen/Passes.h"

#include <range/v3/view.hpp>
#include <svm/Builtin.h>

#include "Assembly/AssemblyStream.h"
#include "Assembly/Block.h"
#include "Assembly/Instruction.h"
#include "Assembly/Value.h"
#include "MIR/CFG.h"
#include "MIR/Instructions.h"
#include "MIR/Module.h"

using namespace scatha;
using namespace Asm;
using namespace ranges::views;

namespace {

struct CGContext {
    CGContext(Asm::AssemblyStream& result): result(result) {}

    void run(mir::Module const& mod);

    void genFunction(mir::Function const& func);
    void genBlock(mir::BasicBlock const& block);
    void genInst(mir::Instruction const& inst);

    void genInstImpl(mir::StoreInst const&);
    void genInstImpl(mir::LoadInst const&);
    void genInstImpl(mir::CopyInst const&);
    void genInstImpl(mir::CallValueInst const&);
    void genInstImpl(mir::CallMemoryInst const&);
    void genInstImpl(mir::CondCopyInst const&);
    void genInstImpl(mir::LISPInst const&);
    void genInstImpl(mir::LEAInst const&);
    void genInstImpl(mir::CompareInst const&);
    void genInstImpl(mir::TestInst const&);
    void genInstImpl(mir::SetInst const&);
    void genInstImpl(mir::UnaryArithmeticInst const&);
    void genInstImpl(mir::ValueArithmeticInst const&);
    void genInstImpl(mir::LoadArithmeticInst const&);
    void genInstImpl(mir::ConversionInst const&);
    void genInstImpl(mir::JumpInst const&);
    void genInstImpl(mir::CondJumpInst const&);
    void genInstImpl(mir::ReturnInst const&);
    void genInstImpl(mir::PhiInst const&);
    void genInstImpl(mir::SelectInst const&);

    LabelID getLabelID(mir::Value const& value) {
        SC_ASSERT(isa<mir::Function>(value) || isa<mir::BasicBlock>(value),
                  "Only addressable values can have labels");
        auto [itr, success] =
            labelIDs.insert({ &value, LabelID{ labelIndexCounter } });
        if (success) {
            ++labelIndexCounter;
        }
        return itr->second;
    }

    Asm::RegisterIndex toRegIdx(mir::Register const* reg) const {
        SC_ASSERT(reg->nodeType() == mir::NodeType::HardwareRegister,
                  "At this point we expect all registers to be hardware "
                  "registers");
        return Asm::RegisterIndex(reg->index());
    }

    Asm::RegisterIndex toRegIdx(mir::Value const* value) const {
        return toRegIdx(cast<mir::Register const*>(value));
    }

    Asm::Value toValue(mir::Value const* value) {
        // clang-format off
         return SC_MATCH (*value) {
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
             [&](mir::Function const& F) -> Asm::Value {
                 return LabelPosition(getLabelID(F),
                                      LabelPosition::Dynamic);
             },
             [](mir::Value const&) -> Asm::Value {
                 SC_UNREACHABLE();
             }
         }; // clang-format on
    }

    void addMetadata(mir::Instruction const& inst) {
        SC_EXPECT(currentBlock);
        currentBlock->back().setMetadata(inst.metadata());
    }

    Asm::AssemblyStream& result;
    Asm::Block* currentBlock = nullptr;
    mir::Function const* currentFunction = nullptr;

    /// Maps basic blocks and functions to label IDs
    utl::hashmap<mir::Value const*, LabelID> labelIDs;
    size_t labelIndexCounter = 0;
};

} // namespace

Asm::AssemblyStream cg::lowerToASM(mir::Module const& mod) {
    Asm::AssemblyStream result;
    CGContext ctx(result);
    ctx.run(mod);
    return result;
}

void CGContext::run(mir::Module const& mod) {
    for (auto& F: mod) {
        genFunction(F);
    }
    result.setDataSection(mod.dataSection());
    result.setMetadata(mod.metadata());
    auto jumpsites = mod.addressPlaceholders() | transform([&](auto p) {
        auto [offset, function] = p;
        return Jumpsite{ offset, getLabelID(*function), 8 };
    }) | ranges::to<std::vector>;
    result.setJumpSites(std::move(jumpsites));
}

void CGContext::genFunction(mir::Function const& F) {
    currentBlock = result.add(Asm::Block(getLabelID(F), std::string(F.name())));
    if (F.visibility() == mir::Visibility::External) {
        currentBlock->setExternallyVisible();
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

template <typename V>
static Asm::MemoryAddress convertAddress(mir::MemoryAddressImpl<V> addr) {
    auto* base = cast<mir::Register const*>(addr.baseAddress());
    auto* factor = cast<mir::Register const*>(addr.dynOffset());
    return Asm::MemoryAddress(RegisterIndex(base->index()),
                              factor ? RegisterIndex(factor->index()) :
                                       Asm::MemoryAddress::InvalidRegisterIndex,
                              addr.offsetFactor(), addr.offsetTerm());
}

static Asm::UnaryArithmeticOperation mapUnaryArithmetic(
    mir::UnaryArithmeticOperation op);

static Asm::ArithmeticOperation mapArithmetic(mir::ArithmeticOperation op);

static Asm::CompareOperation mapCompareOperation(mir::CompareOperation op);

static Asm::Type mapCompareMode(ir::CompareMode mode);

void CGContext::genInst(mir::Instruction const& inst) {
    visit(inst, [&](auto& inst) { genInstImpl(inst); });
}

void CGContext::genInstImpl(mir::StoreInst const& inst) {
    auto dest = convertAddress(inst.address());
    /// We cast to register because we can only move to memory from a register
    auto source = toRegIdx(inst.source());
    currentBlock->insertBack(MoveInst(dest, source, inst.bytewidth()));
    addMetadata(inst);
}

void CGContext::genInstImpl(mir::LoadInst const& inst) {
    auto dest = toRegIdx(inst.dest());
    auto source = convertAddress(inst.address());
    currentBlock->insertBack(MoveInst(dest, source, inst.bytewidth()));
    addMetadata(inst);
}

void CGContext::genInstImpl(mir::CopyInst const& inst) {
    auto dest = toRegIdx(inst.dest());
    auto source = toValue(inst.operandAt(0));
    currentBlock->insertBack(MoveInst(dest, source, inst.bytewidth()));
    addMetadata(inst);
}

void CGContext::genInstImpl(mir::CallValueInst const& inst) {
    // clang-format off
    SC_MATCH (*inst.callee()) {
        [&](mir::Function const& callee) {
            auto asmInst = CallInst(LabelPosition(getLabelID(callee)),
                                    inst.registerOffset());
            currentBlock->insertBack(asmInst);
            addMetadata(inst);
        },
        [&](mir::ForeignFunction const& callee) {
            currentBlock->insertBack(
                CallExtInst(inst.registerOffset(),
                            callee.getFFI()));
            addMetadata(inst);
        },
        [&](mir::Register const& reg) {
            auto asmInst = CallInst(RegisterIndex(reg.index()),
                                    inst.registerOffset());
            currentBlock->insertBack(asmInst);
            addMetadata(inst);
        },
        [&](mir::UndefValue const&) {
            auto asmInst = CallInst(RegisterIndex(255),
                                    inst.registerOffset());
            currentBlock->insertBack(asmInst);
            addMetadata(inst);
        },
        [&](mir::Value const&) {
            SC_UNREACHABLE();
        },
    }; // clang-format on
}

void CGContext::genInstImpl(mir::CallMemoryInst const& inst) {
    auto asmInst =
        CallInst(convertAddress(inst.callee()), inst.registerOffset());
    currentBlock->insertBack(asmInst);
    addMetadata(inst);
}

void CGContext::genInstImpl(mir::CondCopyInst const& inst) {
    auto dest = toRegIdx(inst.dest());
    auto source = toValue(inst.source());
    auto cond = mapCompareOperation(inst.condition());
    currentBlock->insertBack(CMoveInst(cond, dest, source, inst.bytewidth()));
    addMetadata(inst);
}

void CGContext::genInstImpl(mir::LISPInst const& inst) {
    auto dest = toRegIdx(inst.dest());
    auto numBytes = toValue(inst.allocSize());
    currentBlock->insertBack(
        LIncSPInst(dest, std::get<Asm::Value16>(numBytes)));
    addMetadata(inst);
}

void CGContext::genInstImpl(mir::LEAInst const& inst) {
    auto dest = toRegIdx(inst.dest());
    auto address = convertAddress(inst.address());
    currentBlock->insertBack(LEAInst(dest, address));
    addMetadata(inst);
}

void CGContext::genInstImpl(mir::CompareInst const& inst) {
    currentBlock->insertBack(
        Asm::CompareInst(mapCompareMode(inst.mode()), toValue(inst.LHS()),
                         toValue(inst.RHS()), inst.bytewidth()));
    addMetadata(inst);
}

void CGContext::genInstImpl(mir::TestInst const& inst) {
    auto operand = toValue(inst.operand());
    currentBlock->insertBack(
        TestInst(mapCompareMode(inst.mode()), operand, inst.bytewidth()));
    addMetadata(inst);
}

void CGContext::genInstImpl(mir::SetInst const& inst) {
    currentBlock->insertBack(
        SetInst(toRegIdx(inst.dest()), mapCompareOperation(inst.operation())));
    addMetadata(inst);
}

void CGContext::genInstImpl(mir::UnaryArithmeticInst const& inst) {
    SC_ASSERT(inst.dest() == inst.operand(), "Illegal instruction");
    auto operand = toRegIdx(inst.operand());
    auto operation = inst.operation();
    currentBlock->insertBack(UnaryArithmeticInst(mapUnaryArithmetic(operation),
                                                 operand, inst.bytewidth()));
    addMetadata(inst);
}

void CGContext::genInstImpl(mir::ValueArithmeticInst const& inst) {
    SC_ASSERT(inst.dest() == inst.LHS(), "Illegal instruction");
    auto LHS = toRegIdx(inst.LHS());
    auto RHS = toValue(inst.RHS());
    auto operation = mapArithmetic(inst.operation());
    currentBlock->insertBack(
        Asm::ArithmeticInst(operation, LHS, RHS, inst.bytewidth()));
    addMetadata(inst);
}

void CGContext::genInstImpl(mir::LoadArithmeticInst const& inst) {
    SC_ASSERT(inst.dest() == inst.LHS(), "Illegal instruction");
    auto LHS = toRegIdx(inst.LHS());
    auto RHS = convertAddress(inst.RHS());
    auto operation = mapArithmetic(inst.operation());
    currentBlock->insertBack(
        Asm::ArithmeticInst(operation, LHS, RHS, inst.bytewidth()));
    addMetadata(inst);
}

void CGContext::genInstImpl(mir::ConversionInst const& inst) {
    SC_ASSERT(inst.dest() == inst.operand(), "Illegal instruction");
    auto operand = toRegIdx(inst.operand());
    switch (inst.conversion()) {
    case mir::Conversion::Sext:
        currentBlock->insertBack(
            TruncExtInst(operand, Asm::Type::Signed, inst.bitwidth()));
        addMetadata(inst);
        break;
    case mir::Conversion::Fext:
        currentBlock->insertBack(
            TruncExtInst(operand, Asm::Type::Float, inst.bitwidth()));
        addMetadata(inst);
        break;
    case mir::Conversion::Ftrunc:
        currentBlock->insertBack(
            TruncExtInst(operand, Asm::Type::Float, inst.bitwidth()));
        addMetadata(inst);
        break;
    case mir::Conversion::UtoF:
        currentBlock->insertBack(ConvertInst(operand, Asm::Type::Unsigned,
                                             inst.fromBits(), Asm::Type::Float,
                                             inst.toBits()));
        addMetadata(inst);
        break;
    case mir::Conversion::StoF:
        currentBlock->insertBack(ConvertInst(operand, Asm::Type::Signed,
                                             inst.fromBits(), Asm::Type::Float,
                                             inst.toBits()));
        addMetadata(inst);
        break;
    case mir::Conversion::FtoU:
        currentBlock->insertBack(
            ConvertInst(operand, Asm::Type::Float, inst.fromBits(),
                        Asm::Type::Unsigned, inst.toBits()));
        addMetadata(inst);
        break;
    case mir::Conversion::FtoS:
        currentBlock->insertBack(ConvertInst(operand, Asm::Type::Float,
                                             inst.fromBits(), Asm::Type::Signed,
                                             inst.toBits()));
        addMetadata(inst);
        break;
    default:
        SC_UNREACHABLE();
    }
}

void CGContext::genInstImpl(mir::JumpInst const& inst) {
    auto ID = getLabelID(*inst.target());
    currentBlock->insertBack(JumpInst(ID));
    addMetadata(inst);
}

void CGContext::genInstImpl(mir::CondJumpInst const& inst) {
    auto condition = mapCompareOperation(inst.condition());
    auto ID = getLabelID(*inst.target());
    currentBlock->insertBack(JumpInst(condition, ID));
    addMetadata(inst);
}

void CGContext::genInstImpl(mir::ReturnInst const& inst) {
    SC_ASSERT(inst.numOperands() == 0, "Illegal instruction");
    currentBlock->insertBack(ReturnInst());
    addMetadata(inst);
}

void CGContext::genInstImpl(mir::PhiInst const&) {
    SC_UNREACHABLE("Illegal instruction");
}

void CGContext::genInstImpl(mir::SelectInst const&) {
    SC_UNREACHABLE("Illegal instruction");
}

static Asm::UnaryArithmeticOperation mapUnaryArithmetic(
    mir::UnaryArithmeticOperation op) {
    switch (op) {
    case mir::UnaryArithmeticOperation::BitwiseNot:
        return Asm::UnaryArithmeticOperation::BitwiseNot;
    case mir::UnaryArithmeticOperation::LogicalNot:
        return Asm::UnaryArithmeticOperation::LogicalNot;
    case mir::UnaryArithmeticOperation::Negate:
        return Asm::UnaryArithmeticOperation::Negate;
    }
}

static Asm::ArithmeticOperation mapArithmetic(mir::ArithmeticOperation op) {
    switch (op) {
    case mir::ArithmeticOperation::Add:
        return Asm::ArithmeticOperation::Add;
    case mir::ArithmeticOperation::Sub:
        return Asm::ArithmeticOperation::Sub;
    case mir::ArithmeticOperation::Mul:
        return Asm::ArithmeticOperation::Mul;
    case mir::ArithmeticOperation::SDiv:
        return Asm::ArithmeticOperation::SDiv;
    case mir::ArithmeticOperation::UDiv:
        return Asm::ArithmeticOperation::UDiv;
    case mir::ArithmeticOperation::SRem:
        return Asm::ArithmeticOperation::SRem;
    case mir::ArithmeticOperation::URem:
        return Asm::ArithmeticOperation::URem;
    case mir::ArithmeticOperation::FAdd:
        return Asm::ArithmeticOperation::FAdd;
    case mir::ArithmeticOperation::FSub:
        return Asm::ArithmeticOperation::FSub;
    case mir::ArithmeticOperation::FMul:
        return Asm::ArithmeticOperation::FMul;
    case mir::ArithmeticOperation::FDiv:
        return Asm::ArithmeticOperation::FDiv;
    case mir::ArithmeticOperation::LShL:
        return Asm::ArithmeticOperation::LShL;
    case mir::ArithmeticOperation::LShR:
        return Asm::ArithmeticOperation::LShR;
    case mir::ArithmeticOperation::AShL:
        return Asm::ArithmeticOperation::AShL;
    case mir::ArithmeticOperation::AShR:
        return Asm::ArithmeticOperation::AShR;
    case mir::ArithmeticOperation::And:
        return Asm::ArithmeticOperation::And;
    case mir::ArithmeticOperation::Or:
        return Asm::ArithmeticOperation::Or;
    case mir::ArithmeticOperation::XOr:
        return Asm::ArithmeticOperation::XOr;
    }
}

static Asm::CompareOperation mapCompareOperation(mir::CompareOperation op) {
    switch (op) {
    case mir::CompareOperation::Less:
        return Asm::CompareOperation::Less;
    case mir::CompareOperation::LessEq:
        return Asm::CompareOperation::LessEq;
    case mir::CompareOperation::Greater:
        return Asm::CompareOperation::Greater;
    case mir::CompareOperation::GreaterEq:
        return Asm::CompareOperation::GreaterEq;
    case mir::CompareOperation::Equal:
        return Asm::CompareOperation::Eq;
    case mir::CompareOperation::NotEqual:
        return Asm::CompareOperation::NotEq;
    }
}

static Asm::Type mapCompareMode(ir::CompareMode mode) {
    switch (mode) {
    case ir::CompareMode::Signed:
        return Type::Signed;
    case ir::CompareMode::Unsigned:
        return Type::Unsigned;
    case ir::CompareMode::Float:
        return Type::Float;
    }
    SC_UNREACHABLE();
}
