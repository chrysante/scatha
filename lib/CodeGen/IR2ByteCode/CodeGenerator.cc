#include "CodeGen/IR2ByteCode/CodeGenerator.h"

#include <array>

#include <range/v3/view.hpp>
#include <utl/hashmap.hpp>
#include <utl/scope_guard.hpp>

#include "Assembly/AssemblyStream.h"
#include "Assembly/Block.h"
#include "Assembly/Instruction.h"
#include "Assembly/Value.h"
#include "CodeGen/IR2ByteCode/RegisterDescriptor.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"

using namespace scatha;
using namespace Asm;
using namespace cg;

namespace {

struct CodeGenContext {
    explicit CodeGenContext(AssemblyStream& result);

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
    void generate(ir::ExtFunctionCall const&);
    void generate(ir::Return const&);
    void generate(ir::Phi const&);
    void generate(ir::GetElementPointer const&);

    void postprocess();

    /// Used for generating `Store` and `Load` instructions.
    MemoryAddress computeAddress(ir::Value const&);
    /// Used by `computeAddress`
    MemoryAddress computeGep(ir::GetElementPointer const&);

    void generateBigMove(Value dest,
                         Value source,
                         size_t size,
                         Asm::Block* block = nullptr);
    void generateBigMove(Value dest,
                         Value source,
                         size_t size,
                         Block::ConstIterator before,
                         Asm::Block* block = nullptr);

    void placeArguments(std::span<ir::Value const* const> args);
    void getCallResult(ir::Value const& callInst);

    size_t getLabelID(ir::BasicBlock const&);
    size_t getLabelID(ir::Function const&);

    size_t _getLabelIDImpl(ir::Value const&);

    RegisterDescriptor& currentRD() { return *_currentRD; }
    Asm::Block& currentBlock() { return *_currentBlock; }

    AssemblyStream& result;
    Asm::Block* _currentBlock      = nullptr;
    RegisterDescriptor* _currentRD = nullptr;
    size_t labelIndexCounter       = 0;
    /// This just exists to tie the lifetime of the register descriptors to this
    /// context object.
    utl::hashmap<ir::Function const*, std::unique_ptr<RegisterDescriptor>>
        registerDecriptors;
    utl::hashmap<ir::Phi const*, RegisterIndex> phiTargets;
    /// Maps basic blocks and functions to label IDs
    utl::hashmap<ir::Value const*, size_t> labelIndices;
    /// Maps basic blocks to block the assembly stream.
    utl::hashmap<ir::BasicBlock const*, Asm::Block*> blockMap;
};

} // namespace

AssemblyStream cg::codegen(ir::Module const& mod) {
    AssemblyStream result;
    CodeGenContext ctx(result);
    ctx.run(mod);
    return result;
}

static Asm::ArithmeticOperation mapArithmetic(ir::ArithmeticOperation op);

static Asm::CompareOperation mapCompare(ir::CompareOperation op);

CodeGenContext::CodeGenContext(AssemblyStream& result): result(result) {}

void CodeGenContext::run(ir::Module const& mod) {
    for (auto& function: mod.functions()) {
        dispatch(function);
    }
    postprocess();
}

void CodeGenContext::dispatch(ir::Value const& value) {
    visit(value, [this](auto const& node) { generate(node); });
}

void CodeGenContext::generate(ir::Function const& function) {
    auto [itr, success] = registerDecriptors.insert(
        { &function, std::make_unique<RegisterDescriptor>() });
    SC_ASSERT(success, "");
    _currentRD               = itr->second.get();
    utl::scope_guard clearRD = [&] { _currentRD = nullptr; };
    /// Declare parameters.
    for (auto& param: function.parameters()) {
        currentRD().resolve(param);
    }
    _currentBlock =
        result.add(Block(getLabelID(function), std::string(function.name())));
    for (auto& bb: function) {
        dispatch(bb);
    }
}

void CodeGenContext::generate(ir::BasicBlock const& bb) {
    if (!bb.isEntry()) {
        _currentBlock =
            result.add(Block(getLabelID(bb), std::string(bb.name())));
    }
    for (auto& inst: bb) {
        dispatch(inst);
    }
    blockMap.insert({ &bb, &currentBlock() });
    _currentBlock = nullptr;
}

void CodeGenContext::generate(ir::Alloca const& allocaInst) {
    SC_ASSERT(allocaInst.allocatedType()->align() <= 8,
              "We don't support overaligned types just yet.");
    currentBlock().insertBack(
        AllocaInst(currentRD().resolve(allocaInst).get<RegisterIndex>(),
                   currentRD().allocateAutomatic(
                       utl::ceil_divide(allocaInst.allocatedType()->size(),
                                        8))));
}

void CodeGenContext::generate(ir::Store const& store) {
    MemoryAddress const addr = computeAddress(*store.dest());
    Value const src          = [&] {
        if (isa<ir::PointerType>(store.source()->type())) {
            /// Handle the memory -> memory case separately. This is not really
            /// beautiful and can hopefully be refactored in the future. The
            /// following is copy pasted from ir::Load case and slightly
            /// adjusted.
            MemoryAddress const addr = computeAddress(*store.source());
            Value const dest         = currentRD().makeTemporary();
            size_t const size        = store.source()->type()->size();
            generateBigMove(dest, addr, size);
            return dest;
        }
        return currentRD().resolve(*store.source());
    }();
    if (isLiteralValue(src.valueType())) {
        /// `src` is a value and must be stored in temporary register first.
        size_t const size = sizeOf(src.valueType());
        SC_ASSERT(size <= 8, "");
        auto tmp = currentRD().makeTemporary();
        currentBlock().insertBack(MoveInst(tmp, src, size));
        currentBlock().insertBack(MoveInst(addr, tmp, size));
    }
    else {
        generateBigMove(addr, src, store.source()->type()->size());
    }
}

void CodeGenContext::generate(ir::Load const& load) {
    MemoryAddress const addr = computeAddress(*load.address());
    Value const dest         = currentRD().resolve(load);
    size_t const size        = load.type()->size();
    generateBigMove(dest, addr, size);
}

static Asm::Type mapType(ir::Type const* type) {
    if (isa<ir::IntegralType>(type)) {
        /// TODO: Also handle unsigned comparison.
        return Type::Signed;
    }
    if (isa<ir::FloatType>(type)) {
        return Type::Float;
    }
    SC_UNREACHABLE();
}

void CodeGenContext::generate(ir::CompareInst const& cmp) {
    Value const lhs = [&]() -> Value {
        auto resolvedLhs = currentRD().resolve(*cmp.lhs());
        if (!isa<ir::Constant>(cmp.lhs())) {
            SC_ASSERT(
                resolvedLhs.is<RegisterIndex>(),
                "cmp instruction wants a register index as its lhs argument.");
            return resolvedLhs;
        }
        auto tmpRegIdx = currentRD().makeTemporary();
        currentBlock().insertBack(
            MoveInst(tmpRegIdx, resolvedLhs, cmp.lhs()->type()->size()));
        return tmpRegIdx;
    }();
    currentBlock().insertBack(
        Asm::CompareInst(mapType(cmp.type()),
                         lhs,
                         currentRD().resolve(*cmp.rhs())));
    if (true) /// Actually we should check if the users of this cmp instruction
              /// care about having the result in the corresponding register.
              /// Since we don't have use and user lists yet we do this
              /// unconditionally. This should not introduce errors, it's only
              /// inefficient to execute.
              /// TODO: We have user lists now, change this!
    {
        currentBlock().insertBack(
            SetInst(currentRD().resolve(cmp).get<RegisterIndex>(),
                    mapCompare(cmp.operation())));
    }
}

void CodeGenContext::generate(ir::UnaryArithmeticInst const& inst) {
    auto dest               = currentRD().resolve(inst).get<RegisterIndex>();
    auto operand            = currentRD().resolve(*inst.operand());
    auto genUnaryArithmetic = [&](UnaryArithmeticOperation operation) {
        currentBlock().insertBack(
            MoveInst(dest, operand, inst.operand()->type()->size()));
        currentBlock().insertBack(
            UnaryArithmeticInst(operation, mapType(inst.type()), dest));
    };
    switch (inst.operation()) {
    case ir::UnaryArithmeticOperation::Negation:
        currentBlock().insertBack(MoveInst(dest, Value64(0), 8));
        currentBlock().insertBack(ArithmeticInst(ArithmeticOperation::Sub,
                                                 mapType(inst.type()),
                                                 dest,
                                                 operand));
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

void CodeGenContext::generate(ir::ArithmeticInst const& arithmetic) {
    // TODO: Make the move of the source argument conditional?
    auto dest = currentRD().resolve(arithmetic).get<RegisterIndex>();
    currentBlock().insertBack(
        MoveInst(dest, currentRD().resolve(*arithmetic.lhs()), 8));
    currentBlock().insertBack(
        Asm::ArithmeticInst(mapArithmetic(arithmetic.operation()),
                            mapType(arithmetic.type()),
                            dest,
                            currentRD().resolve(*arithmetic.rhs())));
}

// MARK: Terminators

void CodeGenContext::generate(ir::Goto const& gt) {
    currentBlock().insertBack(JumpInst(getLabelID(*gt.target())));
}

void CodeGenContext::generate(ir::Branch const& br) {
    auto const cmpOp = [&] {
        if (auto const* cond = dyncast<ir::CompareInst const*>(br.condition()))
        {
            return mapCompare(cond->operation());
        }
        auto testOp = [&]() -> Value {
            auto cond = currentRD().resolve(*br.condition());
            if (cond.is<RegisterIndex>()) {
                return cond;
            }
            auto tmp = currentRD().makeTemporary();
            currentBlock().insertBack(MoveInst(tmp, cond, 1));
            return tmp;
        }();
        currentBlock().insertBack(
            TestInst(mapType(br.condition()->type()), testOp));
        return CompareOperation::NotEq;
    }();
    currentBlock().insertBack(JumpInst(cmpOp, getLabelID(*br.thenTarget())));
    currentBlock().insertBack(JumpInst(getLabelID(*br.elseTarget())));
}

void CodeGenContext::generate(ir::FunctionCall const& call) {
    placeArguments(call.arguments());
    currentBlock().insertBack(CallInst(getLabelID(*call.function()),
                                       currentRD().numUsedRegisters() + 2));
    getCallResult(call);
}

void CodeGenContext::generate(ir::ExtFunctionCall const& call) {
    placeArguments(call.arguments());
    currentBlock().insertBack(CallExtInst(currentRD().numUsedRegisters() + 2,
                                          call.slot(),
                                          call.index()));
    getCallResult(call);
}

void CodeGenContext::generate(ir::Return const& ret) {
    if (!isa<ir::VoidType>(ret.value()->type())) {
        auto const returnValue = currentRD().resolve(*ret.value());
        RegisterIndex const returnValueTargetLocation = 0;
        if (!returnValue.is<RegisterIndex>() ||
            returnValue.get<RegisterIndex>() != returnValueTargetLocation)
        {
            generateBigMove(returnValueTargetLocation,
                            returnValue,
                            ret.value()->type()->size());
        }
    }
    currentBlock().insertBack(ReturnInst());
}

void CodeGenContext::generate(ir::Phi const& phi) {
    /// We need to find a register index to put the value in that every incoming
    /// path can agree on. Then put the value into that register in every
    /// incoming path. Then make this value resolve to that register index.
    RegisterIndex const target = currentRD().resolve(phi).get<RegisterIndex>();
    [[maybe_unused]] auto [itr, success] = phiTargets.insert({ &phi, target });
    SC_ASSERT(success, "Is this phi node evaluated multiple times?");
}

void CodeGenContext::generate(ir::GetElementPointer const& gep) {
    /// Do nothing here until we have proper register and value descriptors.
    return;
}

void CodeGenContext::postprocess() {
    /// Place the appropriate values for all phi nodes in the corresponding
    /// registers
    for (auto [phi, targetRegIdx]: phiTargets) {
        auto& rd = *registerDecriptors.find(phi->parent()->parent())->second;
        for (auto [pred, value]: phi->arguments()) {
            auto itr = blockMap.find(pred);
            SC_ASSERT(itr != blockMap.end(), "Where is this BB coming from?");
            auto asmBlock = itr->second;
            auto position = [&] {
                if (asmBlock->empty()) {
                    return asmBlock->end();
                }
                auto back = std::prev(asmBlock->end());
                /// Make sure we place our move instruction right before all
                /// jumps ending the basic block.
                while (back->is<JumpInst>()) {
                    if (back == asmBlock->begin()) {
                        return back;
                    }
                    --back;
                }
                return std::next(back);
            }();
            generateBigMove(targetRegIdx,
                            rd.resolve(*value),
                            value->type()->size(),
                            position,
                            asmBlock);
        }
    }
}

MemoryAddress CodeGenContext::computeAddress(ir::Value const& value) {
    if (auto* gep = dyncast<ir::GetElementPointer const*>(&value)) {
        return computeGep(*gep);
    }
    auto destRegIdx = currentRD().resolve(value);
    return MemoryAddress(destRegIdx.get<RegisterIndex>().value());
}

MemoryAddress CodeGenContext::computeGep(ir::GetElementPointer const& gep) {
    size_t offset          = 0;
    ir::Value const* value = &gep;
    while (true) {
        ir::GetElementPointer const* gepPtr =
            dyncast<ir::GetElementPointer const*>(value);
        if (gepPtr == nullptr) {
            break;
        }
        SC_ASSERT(gepPtr->memberIndices().size() <= 1,
                  "Can't generate code for nested accesses yet");
        size_t const memberIndex = gepPtr->memberIndices().size() == 1 ?
                                       gepPtr->memberIndices().front() :
                                       0;
        offset += cast<ir::StructureType const*>(gepPtr->accessedType())
                      ->memberOffsetAt(memberIndex);
        value = gepPtr->basePointer();
    }
    SC_ASSERT(offset <= 0xFF, "Offset too large");
    RegisterIndex const regIdx =
        currentRD().resolve(*value).get<RegisterIndex>();
    if (isa<ir::PointerType>(value->type())) {
        return MemoryAddress(regIdx.value(),
                             MemoryAddress::invalidRegisterIndex,
                             0,
                             offset);
    }
    else {
        /// We are operating on a temporary.
        RegisterIndex const tmp = currentRD().makeTemporary();
        currentBlock().insertBack(AllocaInst(tmp, regIdx));
        return MemoryAddress(tmp.value(),
                             MemoryAddress::invalidRegisterIndex,
                             0,
                             offset);
    }
}

void CodeGenContext::generateBigMove(Value dest,
                              Value source,
                              size_t size,
                              Asm::Block* block) {
    if (!block) {
        block = &currentBlock();
    }
    generateBigMove(dest, source, size, block->end(), block);
}

void CodeGenContext::generateBigMove(Value dest,
                              Value source,
                              size_t size,
                              Block::ConstIterator before,
                              Asm::Block* block) {
    if (!block) {
        block = &currentBlock();
    }
    if (size <= 8) {
        block->insert(before, MoveInst(dest, source, size));
        return;
    }
    // clang-format off
    auto increment = utl::overload{
        [](RegisterIndex& regIdx) {
            regIdx = RegisterIndex(regIdx.value() + 1);
        },
        [](MemoryAddress& addr) {
            addr = MemoryAddress(addr.baseptrRegisterIndex(),
                                 0xFF,
                                 0,
                                 addr.constantInnerOffset() + 8);
        },
        [](auto&) { SC_UNREACHABLE(); }
    };
    // clang-format on
    SC_ASSERT(size % 8 == 0,
              "Probably not always true and this function needs some work.");
    auto insertRange = ranges::views::iota(0u, size / 8) |
                       ranges::views::transform([&](size_t i) {
                           auto move = MoveInst(dest, source, 8);
                           dest.visit(increment);
                           source.visit(increment);
                           return move;
                       }) |
                       ranges::views::common;
    block->insert(before, insertRange);
}

void CodeGenContext::placeArguments(std::span<ir::Value const* const> args) {
    size_t offset = 0;
    for (auto const arg: args) {
        size_t const argSize = arg->type()->size();
        generateBigMove(RegisterIndex(offset),
                        currentRD().resolve(*arg),
                        argSize);
        offset += utl::ceil_divide(argSize, 8);
    }
    /// Increment to actually point to the first move instruction
    size_t const commonOffset = currentRD().numUsedRegisters() + 2;
    Block::Iterator paramLocation =
        std::prev(currentBlock().end(), static_cast<ssize_t>(offset));
    for (size_t i = 0; i < offset; ++i, ++paramLocation) {
        RegisterIndex& moveDestIdx =
            paramLocation->get<MoveInst>().dest().get<RegisterIndex>();
        size_t const rawIndex = moveDestIdx.value();
        moveDestIdx.setValue(commonOffset + rawIndex);
    }
}

void CodeGenContext::getCallResult(ir::Value const& call) {
    if (isa<ir::VoidType>(call.type())) {
        return;
    }
    RegisterIndex const resultLocation = currentRD().numUsedRegisters() + 2;
    RegisterIndex const targetResultLocation =
        currentRD().resolve(call).get<RegisterIndex>();
    if (resultLocation != targetResultLocation) {
        generateBigMove(targetResultLocation,
                        resultLocation,
                        call.type()->size());
    }
}

size_t CodeGenContext::getLabelID(ir::BasicBlock const& bb) {
    return _getLabelIDImpl(bb);
}

size_t CodeGenContext::getLabelID(ir::Function const& fn) {
    return _getLabelIDImpl(fn);
}

size_t CodeGenContext::_getLabelIDImpl(ir::Value const& value) {
    auto [itr, success] = labelIndices.insert({ &value, labelIndexCounter });
    if (success) {
        ++labelIndexCounter;
    }
    return itr->second;
}

static Asm::ArithmeticOperation mapArithmetic(ir::ArithmeticOperation op) {
    // clang-format off
    return UTL_MAP_ENUM(op, Asm::ArithmeticOperation, {
        { ir::ArithmeticOperation::Add,   Asm::ArithmeticOperation::Add  },
        { ir::ArithmeticOperation::Sub,   Asm::ArithmeticOperation::Sub  },
        { ir::ArithmeticOperation::Mul,   Asm::ArithmeticOperation::Mul  },
        { ir::ArithmeticOperation::Div,   Asm::ArithmeticOperation::Div  },
        { ir::ArithmeticOperation::UDiv,  Asm::ArithmeticOperation::Div  },
        { ir::ArithmeticOperation::Rem,   Asm::ArithmeticOperation::Rem  },
        { ir::ArithmeticOperation::URem,  Asm::ArithmeticOperation::Rem  },
        { ir::ArithmeticOperation::LShL,  Asm::ArithmeticOperation::LShL },
        { ir::ArithmeticOperation::LShR,  Asm::ArithmeticOperation::LShR },
        { ir::ArithmeticOperation::AShL,  Asm::ArithmeticOperation::AShL },
        { ir::ArithmeticOperation::AShR,  Asm::ArithmeticOperation::AShR },
        { ir::ArithmeticOperation::And,   Asm::ArithmeticOperation::And  },
        { ir::ArithmeticOperation::Or,    Asm::ArithmeticOperation::Or   },
        { ir::ArithmeticOperation::XOr,   Asm::ArithmeticOperation::XOr  },
    });
    // clang-format on
}

static Asm::CompareOperation mapCompare(ir::CompareOperation op) {
    // clang-format off
    return UTL_MAP_ENUM(op, Asm::CompareOperation, {
        { ir::CompareOperation::Less,      Asm::CompareOperation::Less      },
        { ir::CompareOperation::LessEq,    Asm::CompareOperation::LessEq    },
        { ir::CompareOperation::Greater,   Asm::CompareOperation::Greater   },
        { ir::CompareOperation::GreaterEq, Asm::CompareOperation::GreaterEq },
        { ir::CompareOperation::Equal,     Asm::CompareOperation::Eq        },
        { ir::CompareOperation::NotEqual,  Asm::CompareOperation::NotEq     },
    });
    // clang-format on
}
