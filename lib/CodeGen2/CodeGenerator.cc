#include "CodeGen2/CodeGenerator.h"

#include <utl/hashmap.hpp>
#include <utl/scope_guard.hpp>

#include "Assembly/AssemblyStream.h"
#include "CodeGen2/RegisterDescriptor.h"
#include "IR/Module.h"
#include "IR/CFG.h"
#include "IR/Context.h"

using namespace scatha;
using namespace assembly;
using namespace cg2;

namespace {

struct Context {
    explicit Context(AssemblyStream& result): result(result) {}
    
    void run(ir::Module const& mod);
    
    void dispatch(ir::Value const& value);
    
    void generate(ir::Value const& value) { /* SC_UNREACHABLE(); */ }
    void generate(ir::Function const& function);
    void generate(ir::BasicBlock const& bb);
    void generate(ir::Alloca const&);
    void generate(ir::Store const&);
    void generate(ir::Load const&);
    void generate(ir::CompareInst const&);
    void generate(ir::ArithmeticInst const&);
    void generate(ir::Goto const&);
    void generate(ir::Branch const&);
    
    Label toLabel(ir::BasicBlock const&);
    Label toLabel(ir::Function const&);
    size_t toLabelImpl(ir::Value const&);
    
    RegisterDescriptor& currentRD() { return *_currentRD; }
    AssemblyStream& result;
    RegisterDescriptor* _currentRD = nullptr;
    size_t labelIndexCounter = 0;
    utl::hashmap<ir::Value const*, size_t> labelIndices;
};

} // namespace

assembly::AssemblyStream cg2::codegen(ir::Module const& mod) {
    assembly::AssemblyStream result;
    Context ctx(result);
    ctx.run(mod);
    return result;
}

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
    result << toLabel(function);
    for (auto& bb: function.basicBlocks()) {
        dispatch(bb);
    }
}

void Context::generate(ir::BasicBlock const& bb) {
    if (!bb.isEntry()) {
        result << toLabel(bb);
    }
    for (auto& inst: bb.instructions) {
        dispatch(inst);
    }
}

void Context::generate(ir::Alloca const& allocaInst) {
    /// MARK: Todo list
    /// - Remove necessity for the enterFn instruction: Preallocate e.g. 8mb of registers and never reallocate. This
    ///   way 'all' positive register indices are available for function execution and all we have to do is bump the
    ///   register pointer on function calls. [done]
    /// - Then we still have to solve pointer invalidation on memory reallocation but that should be solvable somehow. [later]
    /// - Once we have both of these we can switch to using real pointers in the VM execution. 
    /// - Then implementation of alloca will be a walk in the park. Just bump the index of the register descriptor by
    ///   ceil_divide(size, reg_size), handle overalignment somehow and return the register pointer plus offset.
    ///   We already have load and store operations (movMR, movRM, etc).
    
    SC_ASSERT(allocaInst.allocatedType()->align() <= 8, "We don't support overaligned types just yet.");
    result << Instruction::storeRegAddress;
    result << currentRD().resolve(allocaInst);
    result << currentRD().allocateAutomatic(utl::ceil_divide(allocaInst.allocatedType()->size(), 8));
}

void Context::generate(ir::Store const& store) {
    auto dest = currentRD().resolve(*store.address());
    auto src = currentRD().resolve(*store.value());
    
    if (std::holds_alternative<assembly::Value64>(src)) {
        /// \p src is a value and must be stored in temporary register first.
        auto tmp = currentRD().makeTemporary();
        result << Instruction::mov << tmp << src;
        result << Instruction::mov << dest << tmp;
    }
    else {
        result << Instruction::mov << dest << src;
    }
}

void Context::generate(ir::Load const& load) {
    result << Instruction::mov;
    result << currentRD().resolve(load);
    result << currentRD().resolve(*load.address());
}

static Instruction mapCmp(ir::Type const* type) {
    if (type->isIntegral()) {
        /// TODO: Also handle unsigned comparison.
        return Instruction::icmp;
    }
    if (type->isFloat()) {
        return Instruction::fcmp;
    }
    SC_UNREACHABLE();
}

void Context::generate(ir::CompareInst const& cmp) {
    result << mapCmp(cmp.operandType()) << currentRD().resolve(*cmp.lhs()) << currentRD().resolve(*cmp.rhs());
}

static Instruction mapArithmetic(ir::ArithmeticOperation op) {
    return UTL_MAP_ENUM(op, Instruction, {
        { ir::ArithmeticOperation::Add,    Instruction::add },
        { ir::ArithmeticOperation::Sub,    Instruction::sub },
        { ir::ArithmeticOperation::Mul,    Instruction::mul },
        { ir::ArithmeticOperation::Div,    Instruction::idiv },
        { ir::ArithmeticOperation::UDiv,   Instruction::div },
        { ir::ArithmeticOperation::Rem,    Instruction::irem },
        { ir::ArithmeticOperation::URem,   Instruction::rem },
        { ir::ArithmeticOperation::ShiftL, Instruction::sl },
        { ir::ArithmeticOperation::ShiftR, Instruction::sr },
        { ir::ArithmeticOperation::And,    Instruction::And },
        { ir::ArithmeticOperation::Or,     Instruction::Or },
        { ir::ArithmeticOperation::XOr,    Instruction::XOr },
    });
}

void Context::generate(ir::ArithmeticInst const& arithmetic) {
    result << mapArithmetic(arithmetic.operation()) << currentRD().resolve(*arithmetic.lhs()) << currentRD().resolve(*arithmetic.rhs());
}

// MARK: Terminators

static Instruction mapCmpToJump(ir::CompareOperation op) {
    return UTL_MAP_ENUM(op, Instruction, {
        { ir::CompareOperation::Less,      Instruction::jl },
        { ir::CompareOperation::LessEq,    Instruction::jle },
        { ir::CompareOperation::Greater,   Instruction::jg },
        { ir::CompareOperation::GreaterEq, Instruction::jge },
        { ir::CompareOperation::Equal,     Instruction::je },
        { ir::CompareOperation::NotEqual,  Instruction::jne },
    });
}

void Context::generate(ir::Goto const& gt) {
    result << Instruction::jmp << toLabel(*gt.target());
}

void Context::generate(ir::Branch const& br) {
    auto const& cond = *cast<ir::CompareInst const*>(br.condition());
    result << mapCmpToJump(cond.operation()) << toLabel(*br.thenTarget());
    result << Instruction::jmp << toLabel(*br.elseTarget());
}

Label Context::toLabel(ir::BasicBlock const& bb) {
    return Label(toLabelImpl(bb), 0);
}

Label Context::toLabel(ir::Function const& fn) {
    return Label(toLabelImpl(fn), 0);
}

size_t Context::toLabelImpl(ir::Value const& value) {
    auto [itr, success] = labelIndices.insert({ &value, labelIndexCounter });
    if (success) { ++labelIndexCounter; }
    return itr->second;
}
