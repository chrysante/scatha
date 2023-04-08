#include "CodeGen/IRToMIR.h"

#include <utl/functional.hpp>

#include "IR/CFG.h"
#include "IR/Module.h"
#include "IR/Type.h"
#include "MIR/CFG.h"
#include "MIR/Module.h"

using namespace scatha;
using namespace cg;

namespace {

struct CodeGenContext {
    explicit CodeGenContext(mir::Module& result): result(result) {}

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
    mir::MemoryAddress computeAddress(ir::Value const&);

    /// Used by `computeAddress`
    mir::MemoryAddress computeGep(ir::GetElementPointer const&);

    void placeArguments(std::span<ir::Value const* const> args);

    void getCallResult(ir::Value const& callInst);

    size_t nextRegIndex() { return regIdx++; }

    mir::Module& result;

    mir::Function* currentFunction = nullptr;
    mir::BasicBlock* currentBlock  = nullptr;

    utl::hashmap<ir::BasicBlock const*, mir::BasicBlock*> basicBlockMap;

    utl::hashmap<ir::Function const*, mir::Function*> functionMap;

    size_t regIdx = 0;
};

} // namespace

mir::Module cg::lowerToMIR(ir::Module const& mod) {
    mir::Module result;
    CodeGenContext ctx(result);
    ctx.run(mod);
    return result;
}

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
    utl::small_vector<mir::Parameter*> params;
    for (auto& param: function.parameters()) {
        size_t const size          = param.type()->size();
        size_t const paramRegCount = utl::ceil_divide(size, 8);
        for (size_t i = 0; i < paramRegCount; ++i) {
            params.push_back(new mir::Parameter(nextRegIndex()));
        }
    }
    currentFunction = new mir::Function(params, std::string(function.name()));
    result.addFunction(currentFunction);
    for (auto& bb: function) {
        dispatch(bb);
    }
}

void CodeGenContext::generate(ir::BasicBlock const& bb) {
    currentBlock = new mir::BasicBlock(std::string(bb.name()));
    currentFunction->pushBack(currentBlock);
    basicBlockMap.insert({ &bb, currentBlock });
    for (auto& inst: bb) {
        dispatch(inst);
    }
}

void CodeGenContext::generate(ir::Alloca const& allocaInst) {
    SC_ASSERT(allocaInst.allocatedType()->align() <= 8,
              "We don't support overaligned types just yet.");
    auto* type          = allocaInst.allocatedType();
    auto* countConstant = cast<ir::IntegralConstant const*>(allocaInst.count());
    size_t count        = countConstant->value().to<size_t>();
    size_t numBytes     = type->size() * count;
    auto* inst          = new mir::Instruction(mir::InstructionType::LIncSP,
                                      nextRegIndex(),
                                               { result.constant(numBytes) });
    currentBlock->pushBack(inst);
}

void CodeGenContext::generate(ir::Store const& store) {
    mir::MemoryAddress const addr = computeAddress(*store.address());
    Value const src               = [&] {
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
