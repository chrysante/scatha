#include "CodeGen/IR2ByteCode/CodeGenerator.h"

#include <array>

#include <range/v3/algorithm.hpp>
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
#include "IR/Type.h"

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
    void generate(ir::ConversionInst const&);
    void generate(ir::CompareInst const&);
    void generate(ir::UnaryArithmeticInst const&);
    void generate(ir::ArithmeticInst const&);
    void generate(ir::Goto const&);
    void generate(ir::Branch const&);
    void generate(ir::Call const&);
    void generate(ir::Return const&);
    void generate(ir::Phi const&);
    void generate(ir::GetElementPointer const&);
    void generate(ir::ExtractValue const&);
    void generate(ir::InsertValue const&);
    void generate(ir::Select const&);

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

    Value convertValue(Value value, Type type, size_t fromBits);

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
    for (auto& function: mod) {
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
    auto* type          = allocaInst.allocatedType();
    auto* countConstant = cast<ir::IntegralConstant const*>(allocaInst.count());
    size_t count        = countConstant->value().to<size_t>();
    size_t numBytes     = utl::round_up(type->size() * count, 8);
    currentBlock().insertBack(
        LIncSPInst(currentRD().resolve(allocaInst).get<RegisterIndex>(),
                   Value16(utl::narrow_cast<uint16_t>(numBytes))));
}

void CodeGenContext::generate(ir::Store const& store) {
    MemoryAddress const addr = computeAddress(*store.address());
    Value const src          = [&] {
        if (isa<ir::PointerType>(store.value()->type())) {
            /// Handle the memory -> memory case separately. This is not really
            /// beautiful and can hopefully be refactored in the future. The
            /// following is copy pasted from ir::Load case and slightly
            /// adjusted.
            MemoryAddress const addr = computeAddress(*store.value());
            Value const dest         = currentRD().makeTemporary();
            size_t const size        = store.value()->type()->size();
            generateBigMove(dest, addr, size);
            return dest;
        }
        return currentRD().resolve(*store.value());
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
        generateBigMove(addr, src, store.value()->type()->size());
    }
}

void CodeGenContext::generate(ir::Load const& load) {
    MemoryAddress const addr = computeAddress(*load.address());
    Value const dest         = currentRD().resolve(load);
    size_t const size        = load.type()->size();
    generateBigMove(dest, addr, size);
}

/// All the moves we insert here are unnecessary, we just don't have a better
/// way of implementing this yet...
void CodeGenContext::generate(ir::ConversionInst const& inst) {
    switch (inst.conversion()) {
    case ir::Conversion::Zext:
        [[fallthrough]];
    case ir::Conversion::Trunc:
        [[fallthrough]];
    case ir::Conversion::Bitcast: {
        auto dest = currentRD().resolve(inst);
        auto op   = currentRD().resolve(*inst.operand());
        currentBlock().insertBack(MoveInst(dest, op, 8));
        break;
    }
    case ir::Conversion::Sext: {
        auto dest = currentRD().resolve(inst);
        auto op   = currentRD().resolve(*inst.operand());
        op = convertValue(op, Type::Signed, inst.operand()->type()->size() * 8);
        currentBlock().insertBack(MoveInst(dest, op, 8));
        break;
    }
    case ir::Conversion::Fext: {
        auto dest = currentRD().resolve(inst);
        auto op   = currentRD().resolve(*inst.operand());
        op = convertValue(op, Type::Float, inst.operand()->type()->size() * 8);
        currentBlock().insertBack(MoveInst(dest, op, 8));
        break;
    }
    case ir::Conversion::Ftrunc: {
        auto dest = currentRD().resolve(inst);
        auto op   = currentRD().resolve(*inst.operand());
        op = convertValue(op, Type::Float, inst.operand()->type()->size() * 8);
        currentBlock().insertBack(MoveInst(dest, op, 8));
        break;
    }
    case ir::Conversion::_count:
        SC_UNREACHABLE();
    }
}

static Asm::Type mapCmpMode(ir::CompareMode mode) {
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

void CodeGenContext::generate(ir::CompareInst const& cmp) {
    auto* operandType = cmp.lhs()->type();
    Value LHS         = [&]() -> Value {
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
    Value RHS          = currentRD().resolve(*cmp.rhs());
    auto const cmpMode = mapCmpMode(cmp.mode());
    if (cmpMode == Type::Signed) {
        auto* intType         = cast<ir::IntegralType const*>(operandType);
        size_t const fromBits = intType->bitWidth();
        LHS                   = convertValue(LHS, Type::Signed, fromBits);
        RHS                   = convertValue(RHS, Type::Signed, fromBits);
    }
    currentBlock().insertBack(
        Asm::CompareInst(cmpMode, LHS, RHS, operandType->size()));
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

static Value widenConstantTo64Bit(Value value) {
    if (value.is<Value8>() || value.is<Value16>() || value.is<Value32>()) {
        return value.as_base<ValueBase>().widen();
    }
    return value;
}

static Value truncConstantTo8Bit(Value value) {
    if (value.is<Value16>() || value.is<Value32>() || value.is<Value64>()) {
        return Value8(value.as_base<ValueBase>().value());
    }
    return value;
}

void CodeGenContext::generate(ir::UnaryArithmeticInst const& inst) {
    auto dest    = currentRD().resolve(inst).get<RegisterIndex>();
    auto operand = widenConstantTo64Bit(currentRD().resolve(*inst.operand()));
    auto genUnaryArithmetic = [&](UnaryArithmeticOperation operation) {
        currentBlock().insertBack(
            MoveInst(dest, operand, inst.operand()->type()->size()));
        currentBlock().insertBack(UnaryArithmeticInst(operation, dest));
    };
    switch (inst.operation()) {
    case ir::UnaryArithmeticOperation::BitwiseNot:
        genUnaryArithmetic(UnaryArithmeticOperation::BitwiseNot);
        break;
    case ir::UnaryArithmeticOperation::LogicalNot:
        genUnaryArithmetic(UnaryArithmeticOperation::LogicalNot);
        break;
    default:
        SC_UNREACHABLE();
    }
}

static bool isSignedOp(ir::ArithmeticOperation op) {
    return op == ir::ArithmeticOperation::SDiv ||
           op == ir::ArithmeticOperation::SRem;
}

[[maybe_unused]] static bool isFloatOp(ir::ArithmeticOperation op) {
    return op == ir::ArithmeticOperation::FAdd ||
           op == ir::ArithmeticOperation::FSub ||
           op == ir::ArithmeticOperation::FMul ||
           op == ir::ArithmeticOperation::FDiv;
}

void CodeGenContext::generate(ir::ArithmeticInst const& arithmetic) {
    // TODO: Make the move of the source argument conditional?
    auto dest           = currentRD().resolve(arithmetic).get<RegisterIndex>();
    auto operation      = mapArithmetic(arithmetic.operation());
    auto LHS            = currentRD().resolve(*arithmetic.lhs());
    auto RHS            = currentRD().resolve(*arithmetic.rhs());
    size_t operandWidth = arithmetic.type()->size();
    bool const isSigned = isSignedOp(arithmetic.operation());
    /// Since all arithmetic operations are on 64 bit types, we need to widen
    /// operands for signed and float operations.
    if (operandWidth < 4) {
        if (isSigned) {
            LHS = convertValue(LHS, Type::Signed, operandWidth);
            RHS = convertValue(RHS, Type::Signed, operandWidth);
        }
        else {
            RHS = widenConstantTo64Bit(RHS);
        }
        operandWidth = 8;
    }
    if (isShift(operation)) {
        RHS = truncConstantTo8Bit(RHS);
    }
    currentBlock().insertBack(MoveInst(dest, LHS, 8));
    currentBlock().insertBack(
        Asm::ArithmeticInst(operation, dest, RHS, operandWidth));
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
        currentBlock().insertBack(TestInst(Type::Unsigned, testOp, 1));
        return CompareOperation::NotEq;
    }();
    currentBlock().insertBack(JumpInst(cmpOp, getLabelID(*br.thenTarget())));
    currentBlock().insertBack(JumpInst(getLabelID(*br.elseTarget())));
}

/// Instruction pointer, register pointer offset and stack pointer
static constexpr size_t NumRegsForMetadata = 3;

void CodeGenContext::generate(ir::Call const& call) {
    placeArguments(call.arguments());
    // clang-format off
    visit(*call.function(), utl::overload{
        [&](ir::Function const& func) {
            currentBlock().insertBack(
                CallInst(getLabelID(func),
                         currentRD().numUsedRegisters() + NumRegsForMetadata));
        },
        [&](ir::ExtFunction const& func) {
            currentBlock().insertBack(
                CallExtInst(currentRD().numUsedRegisters() + NumRegsForMetadata,
                            func.slot(),
                            func.index()));
        },
    }); // clang-format on
    getCallResult(call);
}

void CodeGenContext::generate(ir::Return const& ret) {
    if (!isa<ir::VoidType>(ret.value()->type())) {
        auto const returnValue = currentRD().resolve(*ret.value());
        RegisterIndex const returnValueTargetLocation(0);
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
    bool const allUsersAreLoadsAndStores =
        ranges::all_of(gep.users(), [](ir::User const* user) {
            return isa<ir::Load>(user) || isa<ir::Store>(user);
        });
    if (allUsersAreLoadsAndStores) {
        /// Loads and stores can compute their addresses themselves, so we don't
        /// need to do it here.
        return;
    }
    MemoryAddress address = computeGep(gep);
    RegisterIndex dest    = currentRD().resolve(gep).get<RegisterIndex>();
    currentBlock().insertBack(LEAInst(dest, address));
    return;
}

void CodeGenContext::generate(ir::ExtractValue const& extract) {
    auto baseValue       = currentRD().resolve(*extract.baseValue());
    auto dest            = currentRD().resolve(extract);
    size_t byteOffset    = 0;
    ir::Type const* type = extract.baseValue()->type();
    for (size_t index: extract.memberIndices()) {
        auto* sType = cast<ir::StructureType const*>(type);
        byteOffset += sType->memberOffsetAt(index);
        type = sType->memberAt(index);
    }
    auto const baseRegIdx = baseValue.get<RegisterIndex>();
    auto const sourceRegIdx =
        RegisterIndex(baseRegIdx.value() + byteOffset / 8);
    if (byteOffset % 8 == 0 && type->size() % 8 == 0) {
        generateBigMove(dest, sourceRegIdx, type->size());
    }
    else {
        size_t const size   = type->size();
        size_t const offset = byteOffset % 8;
        SC_ASSERT(size + offset <= 8, "This will need even more work");
        uint64_t const mask = [&] {
            std::array<uint8_t, 8> bytes{};
            for (size_t i = 0; i < size; ++i) {
                bytes[i] = 0xFF;
            }
            return utl::bit_cast<uint64_t>(bytes);
        }();
        currentBlock().insertBack(MoveInst(dest, sourceRegIdx, 8));
        currentBlock().insertBack(ArithmeticInst(ArithmeticOperation::LShR,
                                                 dest,
                                                 Value8(8 * offset),
                                                 8));
        currentBlock().insertBack(
            ArithmeticInst(ArithmeticOperation::And, dest, Value64(mask), 8));
    }
}

void CodeGenContext::generate(ir::InsertValue const& insert) {
    auto original = currentRD().resolve(*insert.baseValue());
    auto dest     = currentRD().resolve(insert);
    generateBigMove(dest, original, insert.type()->size());
    size_t byteOffset    = 0;
    ir::Type const* type = insert.baseValue()->type();
    for (size_t index: insert.memberIndices()) {
        auto* sType = cast<ir::StructureType const*>(type);
        byteOffset += sType->memberOffsetAt(index);
        type = sType->memberAt(index);
    }
    auto source           = currentRD().resolve(*insert.insertedValue());
    auto const baseRegIdx = dest.get<RegisterIndex>();
    auto const destRegIdx = RegisterIndex(baseRegIdx.value() + byteOffset / 8);
    if (byteOffset % 8 == 0 && type->size() % 8 == 0) {
        generateBigMove(destRegIdx, source, type->size());
    }
    else {
        size_t const size   = type->size();
        size_t const offset = byteOffset % 8;
        SC_ASSERT(size + offset <= 8, "This will need even more work");
        uint64_t const destMask = [&] {
            std::array<uint8_t, 8> bytes{};
            for (size_t i = 0; i < size; ++i) {
                bytes[i + offset] = 0xFF;
            }
            return utl::bit_cast<uint64_t>(bytes);
        }();
        currentBlock().insertBack(ArithmeticInst(ArithmeticOperation::And,
                                                 destRegIdx,
                                                 Value64(~destMask),
                                                 8));
        auto tmp = currentRD().makeTemporary();
        currentBlock().insertBack(MoveInst(tmp, source, 8));
        currentBlock().insertBack(ArithmeticInst(ArithmeticOperation::LShL,
                                                 tmp,
                                                 Value8(8 * offset),
                                                 8));
        currentBlock().insertBack(ArithmeticInst(ArithmeticOperation::And,
                                                 tmp,
                                                 Value64(destMask),
                                                 8));
        currentBlock().insertBack(
            ArithmeticInst(ArithmeticOperation::Or, destRegIdx, tmp, 8));
    }
}

void CodeGenContext::generate(ir::Select const& select) {
    auto dest    = currentRD().resolve(select).get<RegisterIndex>();
    auto cond    = currentRD().resolve(*select.condition());
    auto thenVal = currentRD().resolve(*select.thenValue());
    auto elseVal = currentRD().resolve(*select.elseValue());
    currentBlock().insertBack(MoveInst(dest, thenVal, select.type()->size()));
    /// Move `cond` into a register if it not alreay in one.
    if (!cond.is<RegisterIndex>()) {
        auto tmp = currentRD().makeTemporary();
        currentBlock().insertBack(
            MoveInst(tmp, cond, select.condition()->type()->size()));
        cond = tmp;
    }
    currentBlock().insertBack(TestInst(Type::Unsigned, cond, 1));
    currentBlock().insertBack(
        CMoveInst(CompareOperation::Eq, dest, elseVal, select.type()->size()));
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
    auto* basePtr = gep.basePointer();
    RegisterIndex const basePtrRegIdx =
        currentRD().resolve(*basePtr).get<RegisterIndex>();
    RegisterIndex const multiplierRegIdx = [&]() -> RegisterIndex {
        auto* constIndex =
            dyncast<ir::IntegralConstant const*>(gep.arrayIndex());
        if (constIndex && constIndex->value() == 0) {
            return MemoryAddress::InvalidRegisterIndex;
        }
        Value res = currentRD().resolve(*gep.arrayIndex());
        if (res.is<RegisterIndex>()) {
            return res.get<RegisterIndex>();
        }
        auto tmp = currentRD().makeTemporary();
        currentBlock().insertBack(MoveInst(tmp, res, 8));
        return tmp;
    }();
    auto* accType         = gep.inboundsType();
    size_t const elemSize = accType->size();
    size_t innerOffset    = 0;
    for (size_t index: gep.memberIndices()) {
        auto* sType = cast<ir::StructureType const*>(accType);
        innerOffset += sType->memberOffsetAt(index);
        accType = sType->memberAt(index);
    }
    return MemoryAddress(basePtrRegIdx,
                         multiplierRegIdx,
                         elemSize,
                         innerOffset);
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
                                 addr.offsetCountRegisterIndex(),
                                 addr.constantOffsetMultiplier(),
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
    size_t const commonOffset =
        currentRD().numUsedRegisters() + NumRegsForMetadata;
    Block::Iterator paramLocation =
        std::prev(currentBlock().end(), static_cast<ssize_t>(offset));
    for (size_t i = 0; i < offset; ++i, ++paramLocation) {
        RegisterIndex moveDestIdx =
            paramLocation->get<MoveInst>().dest().get<RegisterIndex>();
        size_t const rawIndex = moveDestIdx.value();
        moveDestIdx.setValue(commonOffset + rawIndex);
        paramLocation->get<MoveInst>().setDest(moveDestIdx);
    }
}

void CodeGenContext::getCallResult(ir::Value const& call) {
    if (isa<ir::VoidType>(call.type())) {
        return;
    }
    RegisterIndex const resultLocation(currentRD().numUsedRegisters() +
                                       NumRegsForMetadata);
    RegisterIndex const targetResultLocation(
        currentRD().resolve(call).get<RegisterIndex>());
    if (resultLocation != targetResultLocation) {
        generateBigMove(targetResultLocation,
                        resultLocation,
                        call.type()->size());
    }
}

template <typename T>
static Value64 sextImpl(u64 value) {
    auto origin = static_cast<T>(value);
    auto wide   = static_cast<i64>(origin);
    auto res    = static_cast<u64>(wide);
    return Value64(res);
}

static Value64 fextImpl(u64 value) {
    auto h = static_cast<u32>(value);
    auto f = utl::bit_cast<f32>(h);
    auto d = static_cast<f64>(f);
    auto r = utl::bit_cast<u64>(d);
    return Value64(r);
}

static Value64 ftruncImpl(u64 value) {
    auto d = utl::bit_cast<f64>(value);
    auto f = static_cast<f32>(d);
    auto r = utl::bit_cast<u32>(f);
    return Value64(r);
}

Value CodeGenContext::convertValue(Value value, Type type, size_t fromBits) {
    SC_ASSERT(type == Type::Signed || type == Type::Float, "");
    SC_ASSERT(fromBits <= 64, "");
    if (fromBits == 64) {
        return value;
    }
    // clang-format off
    return utl::visit(utl::overload{
        [&](Value8  v) -> Value {
            SC_ASSERT(type != Type::Float, "");
            if (fromBits == 1) {
                return Value64(v.value() == 0 ? 0 : static_cast<u64>(-1));
            }
            SC_ASSERT(fromBits == 8, "");
            return sextImpl<i8>(v.value());
        },
        [&](Value16 v) -> Value {
            SC_ASSERT(type != Type::Float, "");
            SC_ASSERT(fromBits == 16, "");
            return sextImpl<i16>(v.value());
        },
        [&](Value32 v) -> Value {
            SC_ASSERT(fromBits == 32, "");
            if (type == Type::Signed) {
                return sextImpl<i32>(v.value());
            }
            else {
                return fextImpl(v.value());
            }
        },
        [&](Value64 v) -> Value {
            SC_ASSERT(fromBits == 64, "");
            if (type == Type::Signed) {
                return v;
            }
            else {
                return ftruncImpl(v.value());
            }
        },
        [&](RegisterIndex i) -> Value {
            auto tmp = currentRD().makeTemporary();
            currentBlock().insertBack(MoveInst(tmp, i, 8));
            currentBlock().insertBack(ConvInst(tmp, type, fromBits));
            return tmp;
        },
        [&](MemoryAddress p) -> Value {
            auto tmp = currentRD().makeTemporary();
            currentBlock().insertBack(MoveInst(tmp, p, 8));
            return convertValue(tmp, type, fromBits);
        },
    }, value); // clang-format on
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
        { ir::ArithmeticOperation::SDiv,  Asm::ArithmeticOperation::SDiv },
        { ir::ArithmeticOperation::UDiv,  Asm::ArithmeticOperation::UDiv },
        { ir::ArithmeticOperation::SRem,  Asm::ArithmeticOperation::SRem },
        { ir::ArithmeticOperation::URem,  Asm::ArithmeticOperation::URem },
        { ir::ArithmeticOperation::FAdd,  Asm::ArithmeticOperation::FAdd },
        { ir::ArithmeticOperation::FSub,  Asm::ArithmeticOperation::FSub },
        { ir::ArithmeticOperation::FMul,  Asm::ArithmeticOperation::FMul },
        { ir::ArithmeticOperation::FDiv,  Asm::ArithmeticOperation::FDiv },
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
