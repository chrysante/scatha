#include "CodeGen2/CodeGenerator.h"

#include <utl/hashmap.hpp>
#include <utl/scope_guard.hpp>
#include <utl/ranges.hpp>

#include "Assembly2/AssemblyStream.h"
#include "CodeGen2/RegisterDescriptor.h"
#include "IR/Module.h"
#include "IR/CFG.h"
#include "IR/Context.h"

using namespace scatha;
using namespace asm2;
using namespace cg2;

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
    void generate(ir::ArithmeticInst const&);
    void generate(ir::Goto const&);
    void generate(ir::Branch const&);
    void generate(ir::FunctionCall const&);
    void generate(ir::Return const&);
    void generate(ir::Phi const&);
    
    std::unique_ptr<Label> makeLabel(ir::BasicBlock const&);
    std::unique_ptr<Label> makeLabel(ir::Function const&);
    size_t makeLabelImpl(ir::Value const&);
    
    RegisterDescriptor& currentRD() { return *_currentRD; }
    AssemblyStream& result;
    RegisterDescriptor* _currentRD = nullptr;
    size_t labelIndexCounter = 0;
    utl::hashmap<ir::Value const*, size_t> labelIndices;
};

} // namespace

AssemblyStream cg2::codegen(ir::Module const& mod) {
    AssemblyStream result;
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
    result.add(makeLabel(function));
    for (auto& bb: function.basicBlocks()) {
        dispatch(bb);
    }
}

void Context::generate(ir::BasicBlock const& bb) {
    if (!bb.isEntry()) {
        result.add(makeLabel(bb));
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
    result.add(std::make_unique<StoreRegAddress>(
                   cast<RegisterIndex>(currentRD().resolve(allocaInst)),
                   currentRD().allocateAutomatic(utl::ceil_divide(allocaInst.allocatedType()->size(), 8))));
}

void Context::generate(ir::Store const& store) {
    auto destRegIdx = currentRD().resolve(*store.address());
    auto dest = std::make_unique<MemoryAddress>(cast<RegisterIndex const&>(*destRegIdx).value(), 0, 0);
    auto src = currentRD().resolve(*store.value());
    if (auto* value = dyncast<Value64 const*>(src.get())) {
        /// \p src is a value and must be stored in temporary register first.
        auto tmp = currentRD().makeTemporary();
        auto tmp2 = std::make_unique<RegisterIndex>(*tmp);
        result.add(std::make_unique<MoveInst>(std::move(tmp), std::move(src)));
        result.add(std::make_unique<MoveInst>(std::move(dest), std::move(tmp2)));
    }
    else {
        result.add(std::make_unique<MoveInst>(std::move(dest), std::move(src)));
    }
}

void Context::generate(ir::Load const& load) {
    auto addr = currentRD().resolve(*load.address());
    auto src = std::make_unique<MemoryAddress>(cast<RegisterIndex const&>(*addr).value(), 0, 0);
    result.add(std::make_unique<MoveInst>(
                   currentRD().resolve(load),
                   std::move(src)));
}

static asm2::Type mapType(ir::Type const* type) {
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
    result.add(std::make_unique<asm2::CompareInst>(
                   mapType(cmp.type()),
                   currentRD().resolve(*cmp.lhs()),
                   currentRD().resolve(*cmp.rhs())));
}

static asm2::ArithmeticOperation mapArithmetic(ir::ArithmeticOperation op) {
    return UTL_MAP_ENUM(op, asm2::ArithmeticOperation, {
        { ir::ArithmeticOperation::Add,    asm2::ArithmeticOperation::Add    },
        { ir::ArithmeticOperation::Sub,    asm2::ArithmeticOperation::Sub    },
        { ir::ArithmeticOperation::Mul,    asm2::ArithmeticOperation::Mul    },
        { ir::ArithmeticOperation::Div,    asm2::ArithmeticOperation::Div    },
        { ir::ArithmeticOperation::UDiv,   asm2::ArithmeticOperation::Div    },
        { ir::ArithmeticOperation::Rem,    asm2::ArithmeticOperation::Rem    },
        { ir::ArithmeticOperation::URem,   asm2::ArithmeticOperation::Rem    },
        { ir::ArithmeticOperation::ShiftL, asm2::ArithmeticOperation::_count },
        { ir::ArithmeticOperation::ShiftR, asm2::ArithmeticOperation::_count },
        { ir::ArithmeticOperation::And,    asm2::ArithmeticOperation::_count },
        { ir::ArithmeticOperation::Or,     asm2::ArithmeticOperation::_count },
        { ir::ArithmeticOperation::XOr,    asm2::ArithmeticOperation::_count },
    });
}

void Context::generate(ir::ArithmeticInst const& arithmetic) {
    auto dest = cast<RegisterIndex>(currentRD().resolve(arithmetic));
    auto dest2 = std::make_unique<RegisterIndex>(*dest);
    result.add(std::make_unique<MoveInst>(std::move(dest), currentRD().resolve(*arithmetic.lhs())));
    result.add(std::make_unique<asm2::ArithmeticInst>(
                   mapArithmetic(arithmetic.operation()),
                   mapType(arithmetic.type()),
                   std::move(dest2),
                   currentRD().resolve(*arithmetic.rhs())));
}

// MARK: Terminators

static asm2::CompareOperation mapCompare(ir::CompareOperation op) {
    return UTL_MAP_ENUM(op, asm2::CompareOperation, {
        { ir::CompareOperation::Less,      asm2::CompareOperation::Less      },
        { ir::CompareOperation::LessEq,    asm2::CompareOperation::LessEq    },
        { ir::CompareOperation::Greater,   asm2::CompareOperation::Greater   },
        { ir::CompareOperation::GreaterEq, asm2::CompareOperation::GreaterEq },
        { ir::CompareOperation::Equal,     asm2::CompareOperation::Eq        },
        { ir::CompareOperation::NotEqual,  asm2::CompareOperation::NotEq     },
    });
}

void Context::generate(ir::Goto const& gt) {
    result.add(std::make_unique<JumpInst>(makeLabel(*gt.target())));
}

void Context::generate(ir::Branch const& br) {
    auto const& cond = *cast<ir::CompareInst const*>(br.condition());
    result.add(std::make_unique<JumpInst>(
                   mapCompare(cond.operation()),
                   makeLabel(*br.thenTarget())));
    result.add(std::make_unique<JumpInst>(makeLabel(*br.elseTarget())));
}

void Context::generate(ir::FunctionCall const& call) {
    utl::small_vector<RegisterIndex*> parameterRegisterLocations;
    for (auto const arg: call.arguments()) {
        // TODO: Are the arguments evaluated here?
        auto argRegIdx = std::make_unique<RegisterIndex>(0xFF);
        parameterRegisterLocations.push_back(argRegIdx.get());
        result.add(std::make_unique<MoveInst>(
                       std::move(argRegIdx),
                       currentRD().resolve(*arg)));
    }
    for (auto const& [index, regIdx]: utl::enumerate(parameterRegisterLocations)) {
        regIdx->setValue(currentRD().numUsedRegisters() + 2 + index);
    }
    result.add(std::make_unique<CallInst>(makeLabel(*call.function()), currentRD().numUsedRegisters() + 2));
    if (call.type()->isVoid()) {
        return;
    }
    size_t const resultLocation = currentRD().numUsedRegisters() + 2;
    result.add(std::make_unique<MoveInst>(
                   currentRD().resolve(call),
                   std::make_unique<RegisterIndex>(resultLocation)));
}

void Context::generate(ir::Return const& ret) {
    if (ret.value()) {
        result.add(std::make_unique<MoveInst>(std::make_unique<RegisterIndex>(0), currentRD().resolve(*ret.value())));
    }
    result.add(std::make_unique<ReturnInst>());
}

void Context::generate(ir::Phi const& phi) {
    /// We need to find a register index to put the value in that every incoming path can agree on.
    /// Then put the value into that register in every incoming path.
    /// Then make this value resolve to that register index.
}

std::unique_ptr<Label> Context::makeLabel(ir::BasicBlock const& bb) {
    return std::make_unique<Label>(makeLabelImpl(bb), std::string(bb.name()));
}

std::unique_ptr<Label> Context::makeLabel(ir::Function const& fn) {
    return std::make_unique<Label>(makeLabelImpl(fn), std::string(fn.name()));
}

size_t Context::makeLabelImpl(ir::Value const& value) {
    auto [itr, success] = labelIndices.insert({ &value, labelIndexCounter });
    if (success) { ++labelIndexCounter; }
    return itr->second;
}
