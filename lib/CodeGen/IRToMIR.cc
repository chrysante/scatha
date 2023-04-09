#include "CodeGen/IRToMIR.h"

#include <utl/functional.hpp>

#include "IR/CFG.h"
#include "IR/DataFlow.h"
#include "IR/Module.h"
#include "IR/Type.h"
#include "MIR/CFG.h"
#include "MIR/Module.h"

using namespace scatha;
using namespace cg;

namespace {

struct AddNewInstResult {
    mir::Register* reg;
    mir::Instruction* inst;
};

struct CodeGenContext {
    explicit CodeGenContext(mir::Module& result): result(result) {}

    void run(ir::Module const& mod);

    void declareFunction(ir::Function const& function);

    void genFunction(ir::Function const& function);

    void declareBasicBlock(ir::BasicBlock const& bb);

    void genBasicBlock(ir::BasicBlock const& bb);

    /// \Returns The register that this instruction defines. Might be have been
    /// defined before by another instruction, since we are leaving SSA here.
    void dispatchInst(ir::Instruction const& value);
    void genInst(ir::Instruction const& value) { SC_UNREACHABLE(); }
    void genInst(ir::Alloca const&);
    void genInst(ir::Store const&);
    void genInst(ir::Load const&);
    void genInst(ir::ConversionInst const&);
    void genInst(ir::CompareInst const&);
    void genInst(ir::UnaryArithmeticInst const&);
    void genInst(ir::ArithmeticInst const&);
    void genInst(ir::Goto const&);
    void genInst(ir::Branch const&);
    void genInst(ir::Call const&);
    void genInst(ir::Return const&);
    void genInst(ir::Phi const&);
    void genInst(ir::GetElementPointer const&);
    void genInst(ir::ExtractValue const&);
    void genInst(ir::InsertValue const&);
    void genInst(ir::Select const&);

    void postprocess();

    /// Used for generating `Store` and `Load` instructions.
    mir::MemoryAddress computeAddress(ir::Value const*);

    /// Used by `computeAddress`
    mir::MemoryAddress computeGep(ir::GetElementPointer const*);

    void placeArguments(std::span<ir::Value const* const> args);

    void getCallResult(ir::Value const& callInst);

    void generateMoves(mir::Value* dest, mir::Value* src, size_t size);

    void generateMoves(mir::Value* dest,
                       mir::Value* src,
                       size_t size,
                       mir::BasicBlock::Iterator before);

    mir::Register* genCopy(mir::Register* dest,
                           mir::Value* source,
                           size_t numWords) {
        return genCopy(dest, source, numWords, currentBlock->end());
    }

    mir::Register* genCopy(mir::Register* dest,
                           mir::Value* source,
                           size_t numWords,
                           mir::BasicBlock::ConstIterator before);

    /// Maps IR values to MIR values. In particular:
    /// ```
    /// Functions    -> Functions
    /// Basic Blocks -> Basic Blocks
    /// Instructions -> Registers
    /// Constants    -> Constants
    /// ```
    /// Return type can be explicitly specified. Will trap if specified return
    /// type does not match
    template <typename V = mir::Value>
    V* resolve(ir::Value const* value) {
        return cast<V*>(resolveImpl(value));
    }

    mir::Register* resolve(ir::Instruction const* inst) {
        return resolve<mir::Register>(inst);
    }

    mir::Register* resolve(ir::Parameter const* param) {
        return resolve<mir::Register>(param);
    }

    mir::Function* resolve(ir::Function const* func) {
        return resolve<mir::Function>(func);
    }

    mir::BasicBlock* resolve(ir::BasicBlock const* bb) {
        return resolve<mir::BasicBlock>(bb);
    }

    mir::Value* resolveImpl(ir::Value const* value);

    mir::Register* nextRegistersFor(ir::Value const* value) {
        return nextRegistersFor(numWords(value->type()), value);
    }

    mir::Register* nextRegistersFor(size_t numWords, ir::Value const* liveWith);

    template <mir::InstructionData T = uint64_t>
    mir::Instruction* newInst(mir::Register* dest,
                              mir::InstCode code,
                              utl::small_vector<mir::Value*> operands,
                              T instData = {});

    template <mir::InstructionData T = uint64_t>
    AddNewInstResult addNewInst(mir::InstCode code,
                                mir::Register* dest,
                                utl::small_vector<mir::Value*> operands,
                                T instData = {}) {
        return addNewInst(code,
                          dest,
                          std::move(operands),
                          instData,
                          currentBlock->end());
    }

    template <mir::InstructionData T = uint64_t>
    AddNewInstResult addNewInst(mir::InstCode code,
                                mir::Register* dest,
                                utl::small_vector<mir::Value*> operands,
                                T instData,
                                mir::BasicBlock::ConstIterator before);

    size_t numWords(ir::Type const* type) const {
        return utl::ceil_divide(type->size(), 8);
    }

    mir::Module& result;

    mir::Function* currentFunction = nullptr;
    mir::BasicBlock* currentBlock  = nullptr;

    ir::BasicBlockLiveSets const* currentLiveSets = nullptr;

    utl::small_vector<ir::Phi const*> phiNodes;

    utl::hashmap<ir::Value const*, mir::Value*> valueMap;

    utl::hashmap<ir::Call const*, List<mir::Register>> calleeRegStacks;

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
        declareFunction(function);
    }
    for (auto& function: mod) {
        genFunction(function);
    }
    postprocess();
}

void CodeGenContext::declareFunction(ir::Function const& function) {
    auto* mirFunc = new mir::Function(std::string(function.name()));
    result.addFunction(mirFunc);
    valueMap.insert({ &function, mirFunc });
}

void CodeGenContext::genFunction(ir::Function const& function) {
    regIdx          = 0;
    currentFunction = resolve(&function);
    auto liveSets   = ir::LiveSets::compute(function);
    for (auto& bb: function) {
        declareBasicBlock(bb);
    }
    /// Generate registers for parameters.
    auto* entry     = &function.entry();
    currentLiveSets = &liveSets[entry];
    currentBlock    = resolve(entry);
    for (auto& param: function.parameters()) {
        auto* reg = nextRegistersFor(&param);
        valueMap.insert({ &param, reg });
    }
    for (auto& bb: function) {
        currentLiveSets = &liveSets[&bb];
        genBasicBlock(bb);
    }
}

void CodeGenContext::declareBasicBlock(ir::BasicBlock const& bb) {
    auto* mirBB = new mir::BasicBlock(std::string(bb.name()));
    currentFunction->pushBack(mirBB);
    valueMap.insert({ &bb, mirBB });
}

void CodeGenContext::genBasicBlock(ir::BasicBlock const& bb) {
    currentBlock = resolve(&bb);
    for (auto* pred: bb.predecessors()) {
        currentBlock->addPredecessor(resolve(pred));
    }
    for (auto* succ: bb.successors()) {
        currentBlock->addSuccessor(resolve(succ));
    }
    for (auto& inst: bb) {
        dispatchInst(inst);
    }
}

void CodeGenContext::dispatchInst(ir::Instruction const& inst) {
    visit(inst, [this](auto const& inst) { genInst(inst); });
}

void CodeGenContext::genInst(ir::Alloca const& allocaInst) {
    SC_ASSERT(allocaInst.allocatedType()->align() <= 8,
              "We don't support overaligned types just yet.");
    auto* type          = allocaInst.allocatedType();
    auto* countConstant = cast<ir::IntegralConstant const*>(allocaInst.count());
    size_t count        = countConstant->value().to<size_t>();
    size_t numBytes     = type->size() * count;
    addNewInst(mir::InstCode::LIncSP,
               resolve(&allocaInst),
               { result.constant(numBytes) });
}

void CodeGenContext::genInst(ir::Store const& store) {
    mir::MemoryAddress dest = computeAddress(store.address());
    mir::Value* src         = resolve(store.value());
    size_t numWords = utl::ceil_divide(store.value()->type()->size(), 8);
    for (size_t i = 0; i < numWords; ++i) {
        addNewInst(mir::InstCode::Store,
                   nullptr,
                   { dest.addressRegister(), dest.offsetRegister(), src },
                   dest.constantData());
    }
}

void CodeGenContext::genInst(ir::Load const& load) {
    mir::MemoryAddress src = computeAddress(load.address());
    size_t numWords        = utl::ceil_divide(load.type()->size(), 8);
    auto* dest             = resolve(&load);
    for (size_t i = 0; i < numWords; ++i, dest = dest->next()) {
        addNewInst(mir::InstCode::Load,
                   dest,
                   { src.addressRegister(), src.offsetRegister() },
                   src.constantData());
    }
}

void CodeGenContext::genInst(ir::ConversionInst const& inst) {
    switch (inst.conversion()) {
    case ir::Conversion::Zext:
        [[fallthrough]];
    case ir::Conversion::Trunc:
        [[fallthrough]];
    case ir::Conversion::Bitcast: {
        /// These are no-ops, we just return the original register.
        valueMap.insert({ &inst, resolve(inst.operand()) });
        return;
    }
    case ir::Conversion::Sext:
        [[fallthrough]];
    case ir::Conversion::Fext:
        [[fallthrough]];
    case ir::Conversion::Ftrunc: {
        mir::Value* operand = resolve(inst.operand());
        addNewInst(mir::InstCode::Conversion,
                   resolve(&inst),
                   { operand },
                   inst.conversion());
        return;
    }
    case ir::Conversion::_count:
        SC_UNREACHABLE();
    }
}

void CodeGenContext::genInst(ir::CompareInst const& cmp) {
    auto* lhs = resolve(cmp.lhs());
    auto* rhs = resolve(cmp.rhs());
    addNewInst(mir::InstCode::Compare, nullptr, { lhs, rhs }, cmp.mode());
    addNewInst(mir::InstCode::Set, resolve(&cmp), {}, cmp.operation());
}

void CodeGenContext::genInst(ir::UnaryArithmeticInst const& inst) {
    auto* operand = resolve(inst.operand());
    addNewInst(mir::InstCode::UnaryArithmetic,
               resolve(&inst),
               { operand },
               inst.operation());
}

void CodeGenContext::genInst(ir::ArithmeticInst const& inst) {
    auto* lhs = resolve(inst.lhs());
    auto* rhs = resolve(inst.rhs());
    addNewInst(mir::InstCode::Arithmetic,
               resolve(&inst),
               { lhs, rhs },
               inst.operation());
}

void CodeGenContext::genInst(ir::Goto const& gt) {
    auto* target = resolve(gt.target());
    addNewInst(mir::InstCode::Jump, nullptr, { target });
}

void CodeGenContext::genInst(ir::Branch const& br) {
    auto* cond       = resolve(br.condition());
    auto* thenTarget = resolve(br.thenTarget());
    auto* elseTarget = resolve(br.elseTarget());
    addNewInst(mir::InstCode::Test,
               nullptr,
               { cond },
               mir::CompareMode::Unsigned);
    addNewInst(mir::InstCode::CondJump,
               nullptr,
               { thenTarget },
               mir::CompareOperation::NotEqual);
    addNewInst(mir::InstCode::Jump, nullptr, { elseTarget });
}

void CodeGenContext::genInst(ir::Call const& call) {
    /// Place the arguments in expected registers
    auto& calleeRegs    = calleeRegStacks[&call];
    size_t calleeRegIdx = 0;
    for (auto* irArg: call.arguments()) {
        size_t const numWords = this->numWords(irArg->type());
        auto* dest            = new mir::Register(calleeRegIdx++);
        calleeRegs.push_back(dest);
        for (size_t i = 1; i < numWords; ++i) {
            calleeRegs.push_back(new mir::Register(calleeRegIdx++));
        }
        genCopy(dest, resolve(irArg), numWords);
    }
    // clang-format off
    visit(*call.function(), utl::overload{
        [&](ir::Function const& func) {
            auto* callee = resolve(&func);
            addNewInst(mir::InstCode::Call, nullptr, { callee });
        },
        [&](ir::ExtFunction const& func) {
            addNewInst(mir::InstCode::CallExt, nullptr, {}, mir::ExtFuncAddress{
                .slot = static_cast<uint32_t>(func.slot()),
                .index = static_cast<uint32_t>(func.index())
            });
        },
    }); // clang-format on
    genCopy(resolve(&call), &calleeRegs.front(), numWords(call.type()));
}

void CodeGenContext::genInst(ir::Return const& ret) {
    if (!isa<ir::VoidType>(ret.value()->type())) {
        auto* returnValue = resolve(ret.value());
        genCopy(currentFunction->regBegin().to_address(),
                returnValue,
                numWords(ret.value()->type()));
    }
    addNewInst(mir::InstCode::Return, nullptr, {});
}

void CodeGenContext::genInst(ir::Phi const& phi) {
    /// We just remember the phi node to insert copies later.
    phiNodes.push_back(&phi);
}

void CodeGenContext::genInst(ir::GetElementPointer const& gep) {
    bool const allUsersAreLoadsAndStores =
        ranges::all_of(gep.users(), [](ir::User const* user) {
            return isa<ir::Load>(user) || isa<ir::Store>(user);
        });
    if (allUsersAreLoadsAndStores) {
        /// Loads and stores can compute their addresses themselves, so we don't
        /// need to do it here.
        return;
    }
    mir::MemoryAddress address = computeGep(&gep);
    addNewInst(mir::InstCode::LEA,
               resolve(&gep),
               { address.addressRegister(), address.offsetRegister() },
               address.constantData());
}

void CodeGenContext::genInst(ir::ExtractValue const& extract) {
    SC_DEBUGFAIL();
    //    auto baseValue       = currentRD().resolve(*extract.baseValue());
    //    auto dest            = currentRD().resolve(extract);
    //    size_t byteOffset    = 0;
    //    ir::Type const* type = extract.baseValue()->type();
    //    for (size_t index: extract.memberIndices()) {
    //        auto* sType = cast<ir::StructureType const*>(type);
    //        byteOffset += sType->memberOffsetAt(index);
    //        type = sType->memberAt(index);
    //    }
    //    auto const baseRegIdx = baseValue.get<RegisterIndex>();
    //    auto const sourceRegIdx =
    //        RegisterIndex(baseRegIdx.value() + byteOffset / 8);
    //    if (byteOffset % 8 == 0 && type->size() % 8 == 0) {
    //        generateBigMove(dest, sourceRegIdx, type->size());
    //    }
    //    else {
    //        size_t const size   = type->size();
    //        size_t const offset = byteOffset % 8;
    //        SC_ASSERT(size + offset <= 8, "This will need even more work");
    //        uint64_t const mask = [&] {
    //            std::array<uint8_t, 8> bytes{};
    //            for (size_t i = 0; i < size; ++i) {
    //                bytes[i] = 0xFF;
    //            }
    //            return utl::bit_cast<uint64_t>(bytes);
    //        }();
    //        currentBlock().insertBack(MoveInst(dest, sourceRegIdx, 8));
    //        currentBlock().insertBack(ArithmeticInst(ArithmeticOperation::LShR,
    //                                                 dest,
    //                                                 Value8(8 * offset),
    //                                                 8));
    //        currentBlock().insertBack(
    //            ArithmeticInst(ArithmeticOperation::And, dest, Value64(mask),
    //            8));
    //    }
}

void CodeGenContext::genInst(ir::InsertValue const& insert) {
    SC_DEBUGFAIL();
    //    auto original = currentRD().resolve(*insert.baseValue());
    //    auto dest     = currentRD().resolve(insert);
    //    generateBigMove(dest, original, insert.type()->size());
    //    size_t byteOffset    = 0;
    //    ir::Type const* type = insert.baseValue()->type();
    //    for (size_t index: insert.memberIndices()) {
    //        auto* sType = cast<ir::StructureType const*>(type);
    //        byteOffset += sType->memberOffsetAt(index);
    //        type = sType->memberAt(index);
    //    }
    //    auto source           = currentRD().resolve(*insert.insertedValue());
    //    auto const baseRegIdx = dest.get<RegisterIndex>();
    //    auto const destRegIdx = RegisterIndex(baseRegIdx.value() + byteOffset
    //    / 8); if (byteOffset % 8 == 0 && type->size() % 8 == 0) {
    //        generateBigMove(destRegIdx, source, type->size());
    //    }
    //    else {
    //        size_t const size   = type->size();
    //        size_t const offset = byteOffset % 8;
    //        SC_ASSERT(size + offset <= 8, "This will need even more work");
    //        uint64_t const destMask = [&] {
    //            std::array<uint8_t, 8> bytes{};
    //            for (size_t i = 0; i < size; ++i) {
    //                bytes[i + offset] = 0xFF;
    //            }
    //            return utl::bit_cast<uint64_t>(bytes);
    //        }();
    //        currentBlock().insertBack(ArithmeticInst(ArithmeticOperation::And,
    //                                                 destRegIdx,
    //                                                 Value64(~destMask),
    //                                                 8));
    //        auto tmp = currentRD().makeTemporary();
    //        currentBlock().insertBack(MoveInst(tmp, source, 8));
    //        currentBlock().insertBack(ArithmeticInst(ArithmeticOperation::LShL,
    //                                                 tmp,
    //                                                 Value8(8 * offset),
    //                                                 8));
    //        currentBlock().insertBack(ArithmeticInst(ArithmeticOperation::And,
    //                                                 tmp,
    //                                                 Value64(destMask),
    //                                                 8));
    //        currentBlock().insertBack(
    //            ArithmeticInst(ArithmeticOperation::Or, destRegIdx, tmp, 8));
    //    }
}

void CodeGenContext::genInst(ir::Select const& select) {
    auto* cond            = resolve(select.condition());
    auto* thenVal         = resolve(select.thenValue());
    auto* elseVal         = resolve(select.elseValue());
    size_t const numWords = this->numWords(select.type());
    auto* dest            = resolve(&select);
    addNewInst(mir::InstCode::Test,
               nullptr,
               { cond },
               mir::CompareMode::Unsigned);
    genCopy(dest, thenVal, numWords);
    if (auto* srcReg = dyncast<mir::Register*>(elseVal)) {
        for (size_t i = 0; i < numWords;
             ++i, dest = dest->next(), srcReg = srcReg->next())
        {
            addNewInst(mir::InstCode::CondCopy,
                       dest,
                       { srcReg },
                       mir::CompareOperation::NotEqual);
        }
    }
    else {
        SC_ASSERT(numWords == 1, "");
        addNewInst(mir::InstCode::CondCopy,
                   dest,
                   { elseVal },
                   mir::CompareOperation::NotEqual);
    }
}

void CodeGenContext::postprocess() {
    /// Place the appropriate values for all phi nodes in the corresponding
    /// registers
    for (auto* phi: phiNodes) {
        auto* dest = resolve(phi);
        for (auto [pred, arg]: phi->arguments()) {
            auto* mPred = cast<mir::BasicBlock*>(resolve(pred));
            auto before = mPred->end();
            while (true) {
                auto p = std::prev(before);
                if (!isTerminator(p->instcode())) {
                    break;
                }
                before = p;
            }
            genCopy(dest, resolve(arg), numWords(arg->type()), before);
        }
    }
}

mir::MemoryAddress CodeGenContext::computeAddress(ir::Value const* value) {
    if (auto* gep = dyncast<ir::GetElementPointer const*>(value)) {
        return computeGep(gep);
    }
    auto* dest = resolve(value);
    return mir::MemoryAddress(cast<mir::Register*>(dest));
}

mir::MemoryAddress CodeGenContext::computeGep(
    ir::GetElementPointer const* gep) {
    mir::Register* basePtr = cast<mir::Register*>(resolve(gep->basePointer()));
    mir::Register* dynFactor = [&]() -> mir::Register* {
        auto* constIndex =
            dyncast<ir::IntegralConstant const*>(gep->arrayIndex());
        if (constIndex && constIndex->value() == 0) {
            return nullptr;
        }
        mir::Value* arrayIndex = resolve(gep->arrayIndex());
        if (auto* regArrayIdx = cast<mir::Register*>(arrayIndex)) {
            return regArrayIdx;
        }
        return genCopy(nextRegistersFor(1, gep), arrayIndex, 1);
    }();
    auto* accessedType    = gep->inboundsType();
    size_t const elemSize = accessedType->size();
    size_t innerOffset    = 0;
    for (size_t index: gep->memberIndices()) {
        auto* sType = cast<ir::StructureType const*>(accessedType);
        innerOffset += sType->memberOffsetAt(index);
        accessedType = sType->memberAt(index);
    }
    return mir::MemoryAddress(basePtr,
                              dynFactor,
                              utl::narrow_cast<uint32_t>(elemSize),
                              utl::narrow_cast<uint32_t>(innerOffset));
}

mir::Register* CodeGenContext::genCopy(mir::Register* dest,
                                       mir::Value* source,
                                       size_t numWords,
                                       mir::BasicBlock::ConstIterator before) {
    if (!isa<mir::Register>(source)) {
        SC_ASSERT(numWords == 1,
                  "Can't handle literal values larger than 64 bit");
        addNewInst(mir::InstCode::Copy, dest, { source }, uint64_t(0), before);
        return dest;
    }
    auto* result    = dest;
    auto* sourceReg = cast<mir::Register*>(source);
    for (size_t i = 0; i < numWords;
         ++i, dest = dest->next(), sourceReg = sourceReg->next())
    {
        addNewInst(mir::InstCode::Copy, dest, { source }, uint64_t(0), before);
    }
    return result;
}

mir::Value* CodeGenContext::resolveImpl(ir::Value const* value) {
    auto itr = valueMap.find(value);
    if (itr != valueMap.end()) {
        return itr->second;
    }
    // clang-format off
    return visit(*value, utl::overload{
        [&](ir::Instruction const& inst) {
            SC_ASSERT(!isa<ir::VoidType>(inst.type()), "");
            auto* reg = nextRegistersFor(&inst);
            valueMap.insert({ &inst, reg });
            return reg;
        },
        [&](ir::IntegralConstant const& constant) {
            SC_ASSERT(constant.type()->bitWidth() <= 64, "");
            auto value          = constant.value().to<uint64_t>();
            auto* mirConst      = result.constant(value);
            valueMap.insert({ &constant, mirConst });
            return mirConst;
        },
        [&](ir::FloatingPointConstant const& constant) -> mir::Value* {
            SC_DEBUGFAIL();
        },
        [](ir::Value const& value) -> mir::Value* {
            SC_UNREACHABLE("Everything else shall be forward declared");
        }
    }); // clang-format on
}

mir::Register* CodeGenContext::nextRegistersFor(size_t numWords,
                                                ir::Value const* value) {
    bool liveIn  = currentLiveSets->liveIn.contains(value);
    bool liveOut = currentLiveSets->liveOut.contains(value);
    auto* reg    = new mir::Register(regIdx++);
    if (liveIn) {
        currentBlock->addLiveIn(reg);
    }
    if (liveOut) {
        currentBlock->addLiveOut(reg);
    }
    currentFunction->addRegister(reg);
    for (size_t i = 1; i < numWords; ++i) {
        auto* r = new mir::Register(regIdx++);
        currentFunction->addRegister(r);
        if (liveIn) {
            currentBlock->addLiveIn(r);
        }
        if (liveOut) {
            currentBlock->addLiveOut(r);
        }
    }
    return reg;
}

template <mir::InstructionData T>
mir::Instruction* CodeGenContext::newInst(
    mir::Register* dest,
    mir::InstCode code,
    utl::small_vector<mir::Value*> operands,
    T data) {
    return new mir::Instruction(code, dest, std::move(operands), data);
}

template <mir::InstructionData T>
AddNewInstResult CodeGenContext::addNewInst(
    mir::InstCode code,
    mir::Register* dest,
    utl::small_vector<mir::Value*> operands,
    T data,
    mir::BasicBlock::ConstIterator before) {
    auto* inst = newInst(dest, code, std::move(operands), data);
    currentBlock->insert(before, inst);
    return { .reg = dest, .inst = inst };
}
