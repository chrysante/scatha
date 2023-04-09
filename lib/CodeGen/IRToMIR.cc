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
    
    void genFunction(ir::Function const& function);
    void genBasicBlock(ir::BasicBlock const& bb);
    
    mir::Register* dispatchInst(ir::Instruction const& value);
    mir::Register* genInst(ir::Instruction const& value) { SC_UNREACHABLE(); }
    mir::Register* genInst(ir::Alloca const&);
    mir::Register* genInst(ir::Store const&);
    mir::Register* genInst(ir::Load const&);
    mir::Register* genInst(ir::ConversionInst const&);
    mir::Register* genInst(ir::CompareInst const&);
    mir::Register* genInst(ir::UnaryArithmeticInst const&);
    mir::Register* genInst(ir::ArithmeticInst const&);
    mir::Register* genInst(ir::Goto const&);
    mir::Register* genInst(ir::Branch const&);
    mir::Register* genInst(ir::Call const&);
    mir::Register* genInst(ir::Return const&);
    mir::Register* genInst(ir::Phi const&);
    mir::Register* genInst(ir::GetElementPointer const&);
    mir::Register* genInst(ir::ExtractValue const&);
    mir::Register* genInst(ir::InsertValue const&);
    mir::Register* genInst(ir::Select const&);

    void postprocess();

    /// Used for generating `Store` and `Load` instructions.
    mir::MemoryAddress computeAddress(ir::Value const*);

    /// Used by `computeAddress`
    mir::MemoryAddress computeGep(ir::GetElementPointer const*);

    void placeArguments(std::span<ir::Value const* const> args);

    void getCallResult(ir::Value const& callInst);

    void generateMoves(mir::Value* dest, mir::Value* src, size_t size);
    
    void generateMoves(mir::Value* dest, mir::Value* src, size_t size, mir::BasicBlock::Iterator before);
    
    mir::Instruction* genCopy(mir::Value* source, size_t numBytes);
    
    mir::Value* resolve(ir::Value const* value);
    
    mir::Register* makeTemporary(mir::Value* value);

    size_t nextRegIndex() { return regIdx++; }

    template <mir::InstructionData T = uint64_t>
    mir::Instruction* newInst(mir::InstCode code, utl::small_vector<mir::Value*> operands, T instData = {});
    
    template <mir::InstructionData T = uint64_t>
    mir::Instruction* addNewInst(mir::InstCode code, utl::small_vector<mir::Value*> operands, T instData = {});
    
    mir::Module& result;

    mir::Function* currentFunction = nullptr;
    mir::BasicBlock* currentBlock  = nullptr;

    utl::hashmap<ir::Phi const*, mir::Register*> registerMap;
    
    utl::hashmap<ir::Value const*, mir::Register*> registerMap;
    
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
        genFunction(function);
    }
    postprocess();
}

void CodeGenContext::genFunction(ir::Function const& function) {
    utl::small_vector<mir::Parameter*> params;
    for (size_t index = 0; auto& param: function.parameters()) {
        size_t const size          = param.type()->size();
        size_t const paramRegCount = utl::ceil_divide(size, 8);
        for (size_t i = 0; i < paramRegCount; ++i) {
            params.push_back(new mir::Parameter(nextRegIndex()));
        }
        registerMap[&param] = params[index];
        index += paramRegCount;
    }
    currentFunction = new mir::Function(params, std::string(function.name()));
    result.addFunction(currentFunction);
    for (auto& bb: function) {
        genBasicBlock(bb);
    }
}

void CodeGenContext::genBasicBlock(ir::BasicBlock const& bb) {
    currentBlock = new mir::BasicBlock(std::string(bb.name()));
    currentFunction->pushBack(currentBlock);
    basicBlockMap.insert({ &bb, currentBlock });
    for (auto& inst: bb) {
        auto* reg = dispatchInst(inst);
        if (reg) {
            registerMap[&inst] = reg;
        }
    }
}

mir::Register* CodeGenContext::dispatchInst(ir::Instruction const& inst) {
    return visit(inst, [this](auto const& inst) { return genInst(inst); });
}

mir::Register* CodeGenContext::genInst(ir::Alloca const& allocaInst) {
    SC_ASSERT(allocaInst.allocatedType()->align() <= 8,
              "We don't support overaligned types just yet.");
    auto* type          = allocaInst.allocatedType();
    auto* countConstant = cast<ir::IntegralConstant const*>(allocaInst.count());
    size_t count        = countConstant->value().to<size_t>();
    size_t numBytes     = type->size() * count;
    return addNewInst(mir::InstCode::LIncSP,
                   { result.constant(numBytes) });
}

mir::Register* CodeGenContext::genInst(ir::Store const& store) {
    mir::MemoryAddress dest = computeAddress(store.address());
    mir::Value* src         = resolve(store.value());
    size_t numWords = utl::ceil_divide(store.value()->type()->size(), 8);
    for (size_t i = 0; i < numWords; ++i) {
        auto* inst = addNewInst(mir::InstCode::Store,
                             { dest.addressRegister(), dest.offsetRegister(), src },
                             dest.constantData());
    }
    return nullptr;
}

mir::Register* CodeGenContext::genInst(ir::Load const& load) {
    mir::MemoryAddress src  = computeAddress(load.address());
    size_t numWords = utl::ceil_divide(load.type()->size(), 8);
    mir::Register* result = nullptr;
    for (size_t i = 0; i < numWords; ++i) {
        auto* inst = addNewInst(mir::InstCode::Load,
                             { src.addressRegister(), src.offsetRegister() },
                             src.constantData());
        if (i == 0) {
            result = inst;
        }
    }
    return result;
}

/// All the moves we insert here are unnecessary, we just don't have a better
/// way of implementing this yet...
mir::Register* CodeGenContext::genInst(ir::ConversionInst const& inst) {
    switch (inst.conversion()) {
    case ir::Conversion::Zext:
        [[fallthrough]];
    case ir::Conversion::Trunc:
        [[fallthrough]];
    case ir::Conversion::Bitcast: {
        /// These are no-ops, we just return the original register.
        return dyncast<mir::Register*>(resolve(&inst));
    }
    case ir::Conversion::Sext:
        [[fallthrough]];
    case ir::Conversion::Fext:
        [[fallthrough]];
    case ir::Conversion::Ftrunc: {
        mir::Value* operand = resolve(inst.operand());
        return addNewInst(mir::InstCode::Conversion,
                       { operand },
                       inst.conversion());
    }
    case ir::Conversion::_count:
        SC_UNREACHABLE();
    }
}

mir::Register* CodeGenContext::genInst(ir::CompareInst const& cmp) {
    auto* lhs = resolve(cmp.lhs());
    auto* rhs = resolve(cmp.rhs());
    addNewInst(mir::InstCode::Compare, { lhs, rhs }, cmp.mode());
    return addNewInst(mir::InstCode::Set, {});
}

mir::Register* CodeGenContext::genInst(ir::UnaryArithmeticInst const& inst) {
    auto* operand = resolve(inst.operand());
    return addNewInst(mir::InstCode::UnaryArithmetic, { operand }, inst.operation());
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

mir::Register* CodeGenContext::genInst(ir::ArithmeticInst const& inst) {
    auto* lhs = resolve(inst.lhs());
    auto* rhs = resolve(inst.rhs());
    return addNewInst(mir::InstCode::Arithmetic, { lhs, rhs }, inst.operation());
}

mir::Register* CodeGenContext::genInst(ir::Goto const& gt) {
    auto* target = resolve(gt.target());
    addNewInst(mir::InstCode::Jump, { target });
    return nullptr;
}

mir::Register* CodeGenContext::genInst(ir::Branch const& br) {
    auto* cond = resolve(br.condition());
    auto* thenTarget = resolve(br.thenTarget());
    auto* elseTarget = resolve(br.elseTarget());
    addNewInst(mir::InstCode::Test, { cond }, mir::CompareMode::Unsigned);
    addNewInst(mir::InstCode::CJump, { thenTarget }, mir::CompareOperation::NotEqual);
    addNewInst(mir::InstCode::Jump, { elseTarget });
    return nullptr;
}

/// Instruction pointer, register pointer offset and stack pointer
static constexpr size_t NumRegsForMetadata = 3;

mir::Register* CodeGenContext::genInst(ir::Call const& call) {
    /// Place the arguments in expected registers
    for (auto* irArg: call.arguments()) {
        auto* arg = resolve(irArg);
        // FIXME: Correct register indices
        genCopy(arg, irArg->type()->size());
    }
    // clang-format off
    visit(*call.function(), utl::overload{
        [&](ir::Function const& func) {
            auto* callee = resolve(&func);
            addNewInst(mir::InstCode::Call, { callee });
        },
        [&](ir::ExtFunction const& func) {
            addNewInst(mir::InstCode::CallExt, {}, mir::ExtFuncAddress{
                .slot = static_cast<uint32_t>(func.slot()),
                .index = static_cast<uint32_t>(func.index())
            });
        },
    }); // clang-format on
    
    // How to get the call result???
    SC_DEBUGFAIL();
    
//    size_t numWords = utl::ceil_divide(call.type()->size(), 8);
//    for (size_t i = 0; i < numWords; ++i, argReg = argReg->next()) {
//        auto* copy = new mir::Instruction(mir::InstCode::Copy,
//                                          -1, // FIXME: Need correct register index
//                                          { argReg });
//        currentBlock->pushBack(copy);
//    }
//
//
//    getCallResult(call);
}

mir::Register* CodeGenContext::genInst(ir::Return const& ret) {
    if (!isa<ir::VoidType>(ret.value()->type())) {
        auto* returnValue = resolve(ret.value());
        genCopy(returnValue, ret.value()->type()->size());
    }
    return nullptr;
}

mir::Register* CodeGenContext::genInst(ir::Phi const& phi) {
    /// We need to find a register index to put the value in that every incoming
    /// path can agree on. Then put the value into that register in every
    /// incoming path. Then make this value resolve to that register index.
    RegisterIndex const target = currentRD().resolve(phi).get<RegisterIndex>();
    [[maybe_unused]] auto [itr, success] = phiTargets.insert({ &phi, target });
    SC_ASSERT(success, "Is this phi node evaluated multiple times?");
}

mir::Register* CodeGenContext::genInst(ir::GetElementPointer const& gep) {
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

mir::Register* CodeGenContext::genInst(ir::ExtractValue const& extract) {
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

mir::Register* CodeGenContext::genInst(ir::InsertValue const& insert) {
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

mir::Register* CodeGenContext::genInst(ir::Select const& select) {
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

mir::MemoryAddress CodeGenContext::computeAddress(ir::Value const* value) {
    if (auto* gep = dyncast<ir::GetElementPointer const*>(&value)) {
        return computeGep(*gep);
    }
    auto* dest = resolve(value);
    return mir::MemoryAddress(dest);
}

mir::MemoryAddress CodeGenContext::computeGep(ir::GetElementPointer const* gep) {
    mir::Value* basePtr = resolve(gep.basePointer());
    mir::Value* dynFactor = [&]() -> mir::Value* {
        auto* constIndex =
            dyncast<ir::IntegralConstant const*>(gep.arrayIndex());
        if (constIndex && constIndex->value() == 0) {
            return nullptr;
        }
        mir::Value* arrayIndex = resolve(gep.arrayIndex());
        if (isa<Register>(arrayIndex)) {
            return arrayIndex;
        }
        return new mir::Instruction(mir::InstCode::Copy, nextRegIndex(), { arrayIndex });
    }();
    auto* accessedType    = gep.inboundsType();
    size_t const elemSize = accessedType->size();
    size_t innerOffset    = 0;
    for (size_t index: gep.memberIndices()) {
        auto* sType  = cast<ir::StructureType const*>(accessedType);
        innerOffset += sType->memberOffsetAt(index);
        accessedType = sType->memberAt(index);
    }
    return mir::MemoryAddress(basePtr,
                              dynFactor,
                              elemSize,
                              innerOffset);
}

//void CodeGenContext::generateMoves(mir::Value* dest,
//                                   mir::Value* src,
//                                   size_t size) {
//    generateMoves(dest, src, size, currentBlock->end());
//}
//
//void CodeGenContext::generateMoves(mir::Value* dest,
//                                   mir::Value* src,
//                                   size_t size,
//                                   mir::BasicBlock::Iterator before) {
//    for (size_t i = 0; i < utl::ceil_divide(size, 8); ++i) {
//        auto* move = new mir::Instruction(mir::InstCode::);
//        currentBlock->insert(before, move);
//    }
//}

mir::Instruction* CodeGenContext::genCopy(mir::Value* source, size_t numBytes) {
    size_t numWords = utl::ceil_divide(numBytes, 8);
    if (auto* reg = dyncast<mir::Register*>(source)) {
        size_t const regIdx = reg->index();
        mir::Instruction* result = nullptr;
        for (size_t i = 0; i < numWords; ++i, reg = reg->next()) {
            SC_ASSERT(reg->index() == regIdx + i, "We expect the registers to be consecutive");
            auto* copy = new mir::Instruction(mir::InstCode::Copy,
                                              nextRegIndex(),
                                              { reg });
            if (i == 0) {
                result = copy;
            }
            currentBlock->pushBack(copy);
        }
        return result;
    }
    else {
        SC_ASSERT(numWords == 1, "Can't handle literal value larger than 64 bit");
        auto* copy = new mir::Instruction(mir::InstCode::Copy,
                                          nextRegIndex(),
                                          { source });
        currentBlock->pushBack(copy);
        return copy;
    }
}

mir::Value* CodeGenContext::resolve(ir::Value const* value) {
    return visit(*value, utl::overload{
        [&](ir::Instruction const& inst) {
            auto itr = registerMap.find(&inst);
            SC_ASSERT(itr != registerMap.end(), "Not found");
            return itr->second;
        },
        [&](ir::Parameter const& param) {
            auto itr = registerMap.find(&param);
            SC_ASSERT(itr != registerMap.end(), "Not found");
            return itr->second;
        },
        [&](ir::BasicBlock const& BB) {
            auto itr = basicBlockMap.find(&BB);
            SC_ASSERT(itr != basicBlockMap.end(), "Not found");
            return itr->second;
        },
        [&](ir::IntegralConstant const& constant) {
            SC_ASSERT(constant.type()->bitWidth() <= 64);
            return constant.value().to<uint64_t>();
        },
        [&](ir::FloatingPointConstant const& constant) {
            SC_DEBUGFAIL();
//            SC_ASSERT(constant.type()->bitWidth() <= 64);
//            return constant.value().to<uint64_t>();
        },
        [](ir::Value const& value) { SC_UNREACHABLE(); }
    });
}

mir::Register* CodeGenContext::makeTemporary(mir::Value* value) {
    auto* copy = new mir::Instruction(mir::InstCode::Copy, nextRegIndex(), { value });
    currentBlock->pushBack(copy);
    return copy;
}

template <mir::InstructionData T>
mir::Instruction* CodeGenContext::newInst(mir::InstCode code,
                                               utl::small_vector<mir::Value*> operands,
                                               T data) {
    return new mir::Instruction(code, nextRegIndex(), std::move(operands), data);
}

template <mir::InstructionData T>
mir::Instruction* CodeGenContext::addNewInst(mir::InstCode code,
                                          utl::small_vector<mir::Value*> operands,
                                          T data) {
    auto* inst = newInst(code, std::move(operands), data);
    currentBlock->pushBack(inst);
    return inst;
}
