#include "CodeGen/CodeGenerator.h"

#include <array>

#include <utl/hashmap.hpp>
#include <utl/scope_guard.hpp>
#include <utl/ranges.hpp>

#include "Assembly/AssemblyStream.h"
#include "CodeGen/RegisterDescriptor.h"
#include "IR/Module.h"
#include "IR/CFG.h"
#include "IR/Context.h"

using namespace scatha;
using namespace Asm;
using namespace cg;

namespace {

struct Context {
    explicit Context(AssemblyStream& result): result(result) {}
    
    void run(ir::Module const& mod);
    
    void dispatch(ir::Value const& value);
    
    void generate(ir::Value const& value) { SC_UNREACHABLE(); }
    void generate(ir::Function const& function);
    void generate(ir::BasicBlock const& bb);
    void generate(ir::Alloca const&);
    void generate(ir::Store const&);
    void generate(ir::Load const&);
    void generate(ir::CompareInst const&);
    void generate(ir::UnaryArithmeticInst const&);
    void generate(ir::ArithmeticInst const&);
    void generate(ir::Goto const&);
    void generate(ir::Branch const&);
    void generate(ir::FunctionCall const&);
    void generate(ir::Return const&);
    void generate(ir::Phi const&);
    void generate(ir::GetElementPointer const&);
    
    Label makeLabel(ir::BasicBlock const&);
    Label makeLabel(ir::Function const&);
    size_t makeLabelImpl(ir::Value const&);
    
    RegisterDescriptor& currentRD() { return *_currentRD; }
    AssemblyStream& result;
    RegisterDescriptor* _currentRD = nullptr;
    size_t labelIndexCounter = 0;
    utl::hashmap<ir::Value const*, size_t> labelIndices;
    utl::hashmap<ir::BasicBlock const*, std::array<AssemblyStream::Iterator, 2>> bbInstRanges;
};

} // namespace

AssemblyStream cg::codegen(ir::Module const& mod) {
    AssemblyStream result;
    Context ctx(result);
    ctx.run(mod);
    return result;
}

static Asm::ArithmeticOperation mapArithmetic(ir::ArithmeticOperation op);

static Asm::CompareOperation mapCompare(ir::CompareOperation op);

void Context::run(ir::Module const& mod) {
    for (auto& function: mod.functions()) {
        dispatch(function);
    }
}

void Context::dispatch(ir::Value const& value) {
    visit(value, [this](auto const& node) { generate(node); });
}

void Context::generate(ir::Function const& function) {
    RegisterDescriptor rd;
    _currentRD = &rd;
    utl::scope_guard clearRD = [&]{ _currentRD = nullptr; };
    /// Declare parameters.
    for (auto& param: function.parameters()) { rd.resolve(param); }
    result.add(makeLabel(function));
    for (auto& bb: function.basicBlocks()) {
        dispatch(bb);
    }
}

void Context::generate(ir::BasicBlock const& bb) {
    auto const beforeBegin = result.begin();
    if (!bb.isEntry()) {
        result.add(makeLabel(bb));
    }
    for (auto& inst: bb.instructions) {
        dispatch(inst);
    }
    auto const backItr = result.backItr();
    bbInstRanges.insert({ &bb, { std::next(beforeBegin), backItr } });
}

void Context::generate(ir::Alloca const& allocaInst) {
    SC_ASSERT(allocaInst.allocatedType()->align() <= 8,
              "We don't support overaligned types just yet.");
    result.add(AllocaInst(currentRD().resolve(allocaInst).get<RegisterIndex>(),
                          currentRD().allocateAutomatic(utl::ceil_divide(allocaInst.allocatedType()->size(), 8))));
}

void Context::generate(ir::Store const& store) {
    auto destRegIdx = currentRD().resolve(*store.address());
    auto dest = MemoryAddress(destRegIdx.get<RegisterIndex>().value(), 0, 0);
    auto src = currentRD().resolve(*store.value());
    if (src.is<Value64>()) {
        /// \p src is a value and must be stored in temporary register first.
        auto tmp = currentRD().makeTemporary();
        result.add(MoveInst(tmp, src));
        result.add(MoveInst(dest, tmp));
    }
    else {
        result.add(MoveInst(dest, src));
    }
}

void Context::generate(ir::Load const& load) {
    auto addr = currentRD().resolve(*load.address());
    auto src = MemoryAddress(addr.get<RegisterIndex>().value(), 0, 0);
    result.add(MoveInst(currentRD().resolve(load), src));
}

static Asm::Type mapType(ir::Type const* type) {
    if (type->isIntegral()) {
        /// TODO: Also handle unsigned comparison.
        return Type::Signed;
    }
    if (type->isFloat()) {
        return Type::Float;
    }
    SC_UNREACHABLE();
}

void Context::generate(ir::CompareInst const& cmp) {
    Value const lhs = [&]() -> Value {
        auto resolvedLhs = currentRD().resolve(*cmp.lhs());
        if (!isa<ir::Constant>(cmp.lhs())) {
            SC_ASSERT(resolvedLhs.is<RegisterIndex>(), "cmp instruction wants a register index as its lhs argument.");
            return resolvedLhs;
        }
        auto tmpRegIdx = currentRD().makeTemporary();
        result.add(MoveInst(tmpRegIdx, resolvedLhs));
        return tmpRegIdx;
    }();
    result.add(Asm::CompareInst(mapType(cmp.type()), lhs, currentRD().resolve(*cmp.rhs())));
    if (true) /// Actually we should check if the users of this cmp instruction care about having the result in the
              /// corresponding register. Since we don't have use and user lists yet we do this unconditionally. This
              /// should not introduce errors, it's only inefficient to execute.
    {
        result.add(SetInst(currentRD().resolve(cmp).get<RegisterIndex>(), mapCompare(cmp.operation())));
    }
}

void Context::generate(ir::UnaryArithmeticInst const& inst) {
    auto dest = currentRD().resolve(inst).get<RegisterIndex>();
    auto operand = currentRD().resolve(*inst.operand());
    auto genUnaryArithmetic = [&](UnaryArithmeticOperation operation) {
        result.add(MoveInst(dest, operand));
        result.add(UnaryArithmeticInst(operation, mapType(inst.type()), dest));
    };
    switch (inst.operation()) {
    case ir::UnaryArithmeticOperation::Promotion:
        break; /// At this point promotion is a no-op.
    case ir::UnaryArithmeticOperation::Negation:
        result.add(MoveInst(dest, Value64(0)));
        result.add(ArithmeticInst(ArithmeticOperation::Sub, mapType(inst.type()), dest, operand));
        break;
    case ir::UnaryArithmeticOperation::BitwiseNot:
        genUnaryArithmetic(UnaryArithmeticOperation::BitwiseNot);
        break;
    case ir::UnaryArithmeticOperation::LogicalNot:
        genUnaryArithmetic(UnaryArithmeticOperation::LogicalNot);
        break;
    default: SC_UNREACHABLE();
    }
}

void Context::generate(ir::ArithmeticInst const& arithmetic) {
    // TODO: Make the move of the source argument conditional?
    auto dest = currentRD().resolve(arithmetic).get<RegisterIndex>();
    result.add(MoveInst(dest, currentRD().resolve(*arithmetic.lhs())));
    result.add(Asm::ArithmeticInst(mapArithmetic(arithmetic.operation()),
                                    mapType(arithmetic.type()),
                                    dest,
                                    currentRD().resolve(*arithmetic.rhs())));
}

// MARK: Terminators

void Context::generate(ir::Goto const& gt) {
    result.add(JumpInst(makeLabel(*gt.target()).id()));
}

void Context::generate(ir::Branch const& br) {
    auto const cmpOp = [&]{
        if (auto const* cond = dyncast<ir::CompareInst const*>(br.condition())) {
            return mapCompare(cond->operation());
        }
        auto testOp = [&]() -> Value {
            auto cond = currentRD().resolve(*br.condition());
            if (cond.is<RegisterIndex>()) {
                return cond;
            }
            auto tmp = currentRD().makeTemporary();
            result.add(MoveInst(tmp, cond));
            return tmp;
        }();
        result.add(TestInst(mapType(br.condition()->type()), testOp));
        return CompareOperation::NotEq;
    }();
    result.add(JumpInst(cmpOp, makeLabel(*br.thenTarget()).id()));
    result.add(JumpInst(makeLabel(*br.elseTarget()).id()));
}

void Context::generate(ir::FunctionCall const& call) {
    utl::small_vector<AssemblyStream::Iterator> parameterRegIdxLocations;
    for (auto const arg: call.arguments()) {
        // TODO: Are the arguments evaluated here?
        auto argRegIdx = RegisterIndex(0xFF);
        result.add(MoveInst(argRegIdx, currentRD().resolve(*arg)));
        parameterRegIdxLocations.push_back(result.backItr());
    }
    for (auto const& [index, regIdx]: utl::enumerate(parameterRegIdxLocations)) {
        regIdx->get<MoveInst>()
            .dest()
            .get<RegisterIndex>()
            .setValue(currentRD().numUsedRegisters() + 2 + index);
    }
    result.add(CallInst(makeLabel(*call.function()).id(), currentRD().numUsedRegisters() + 2));
    if (call.type()->isVoid()) {
        return;
    }
    RegisterIndex const resultLocation = currentRD().numUsedRegisters() + 2;
    RegisterIndex const targetResultLocation = currentRD().resolve(call).get<RegisterIndex>();
    if (resultLocation != targetResultLocation) {
        result.add(MoveInst(targetResultLocation, resultLocation));
    }
}

void Context::generate(ir::Return const& ret) {
    if (ret.value()) {
        auto const returnValue = currentRD().resolve(*ret.value());
        RegisterIndex const returnValueTargetLocation = 0;
        if (!returnValue.is<RegisterIndex>() ||
            returnValue.get<RegisterIndex>() != returnValueTargetLocation)
        {
            result.add(MoveInst(returnValueTargetLocation, returnValue));
        }
    }
    result.add(ReturnInst());
}

void Context::generate(ir::Phi const& phi) {
    /// We need to find a register index to put the value in that every incoming path can agree on.
    /// Then put the value into that register in every incoming path.
    /// Then make this value resolve to that register index.
    RegisterIndex const target = currentRD().resolve(phi).get<RegisterIndex>();
    for (auto& [pred, value]: phi.arguments) {
        auto [begin, back] = [&, pred = pred]{
            auto itr = bbInstRanges.find(pred);
            SC_ASSERT(itr != bbInstRanges.end(), "Where is this bb coming from?");
            return itr->second;
        }();
        /// Make sure we place our move instruction right before all jumps ending the basic block.
        while (back->is<JumpInst>() && back != begin) { --back; }
        result.insert(std::next(back), MoveInst(target, currentRD().resolve(*value)));
    }
}

void Context::generate(ir::GetElementPointer const& gep) {
    /// Should we really generate arithmetic instructions here or should we perform offset calculation in the memory access??
    auto idx = currentRD().resolve(gep);
    result.add(MoveInst(idx, currentRD().resolve(*gep.basePointer())));
    size_t const offset = static_cast<ir::StructureType const*>(gep.accessedType())->memberOffsetAt(gep.offsetIndex());
    result.add(ArithmeticInst(ArithmeticOperation::Add, Type::Unsigned, idx, Value64(offset)));
}

Label Context::makeLabel(ir::BasicBlock const& bb) {
    return Label(makeLabelImpl(bb), std::string(bb.name()));
}

Label Context::makeLabel(ir::Function const& fn) {
    return Label(makeLabelImpl(fn), std::string(fn.name()));
}

size_t Context::makeLabelImpl(ir::Value const& value) {
    auto [itr, success] = labelIndices.insert({ &value, labelIndexCounter });
    if (success) { ++labelIndexCounter; }
    return itr->second;
}

static Asm::ArithmeticOperation mapArithmetic(ir::ArithmeticOperation op) {
    return UTL_MAP_ENUM(op, Asm::ArithmeticOperation, {
        { ir::ArithmeticOperation::Add,    Asm::ArithmeticOperation::Add },
        { ir::ArithmeticOperation::Sub,    Asm::ArithmeticOperation::Sub },
        { ir::ArithmeticOperation::Mul,    Asm::ArithmeticOperation::Mul },
        { ir::ArithmeticOperation::Div,    Asm::ArithmeticOperation::Div },
        { ir::ArithmeticOperation::UDiv,   Asm::ArithmeticOperation::Div },
        { ir::ArithmeticOperation::Rem,    Asm::ArithmeticOperation::Rem },
        { ir::ArithmeticOperation::URem,   Asm::ArithmeticOperation::Rem },
        { ir::ArithmeticOperation::ShiftL, Asm::ArithmeticOperation::ShL },
        { ir::ArithmeticOperation::ShiftR, Asm::ArithmeticOperation::ShR },
        { ir::ArithmeticOperation::And,    Asm::ArithmeticOperation::And },
        { ir::ArithmeticOperation::Or,     Asm::ArithmeticOperation::Or  },
        { ir::ArithmeticOperation::XOr,    Asm::ArithmeticOperation::XOr },
    });
}

static Asm::CompareOperation mapCompare(ir::CompareOperation op) {
    return UTL_MAP_ENUM(op, Asm::CompareOperation, {
        { ir::CompareOperation::Less,      Asm::CompareOperation::Less      },
        { ir::CompareOperation::LessEq,    Asm::CompareOperation::LessEq    },
        { ir::CompareOperation::Greater,   Asm::CompareOperation::Greater   },
        { ir::CompareOperation::GreaterEq, Asm::CompareOperation::GreaterEq },
        { ir::CompareOperation::Equal,     Asm::CompareOperation::Eq        },
        { ir::CompareOperation::NotEqual,  Asm::CompareOperation::NotEq     },
    });
}
