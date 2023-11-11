#include "CodeGen/Passes.h"

#include <vector>

#include <range/v3/algorithm.hpp>
#include <range/v3/numeric.hpp>
#include <svm/VirtualPointer.h>
#include <utl/functional.hpp>

#include "Common/Ranges.h"
#include "IR/CFG.h"
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

    /// Used for generating `Store` and `Load` instructions.
    mir::MemoryAddress computeAddress(ir::Value const*);

    /// Used by `computeAddress`
    mir::MemoryAddress computeGep(ir::GetElementPointer const*);

    /// \Returns The register after \p dest
    template <typename R>
    R* genCopy(R* dest, mir::Value* source, size_t numBytes) {
        return genCopy(dest, source, numBytes, currentBlock->end());
    }

    /// \overload
    template <typename R>
    R* genCopy(R* dest,
               mir::Value* source,
               size_t numBytes,
               mir::BasicBlock::ConstIterator before,
               mir::InstCode code = mir::InstCode::Copy,
               uint64_t instData = 0);

    ///
    mir::CompareOperation readCondition(ir::Value const* condition);

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
        return cast_or_null<V*>(resolveImpl(value));
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

    /// \Returns If resolved value already is in a register, that register.
    /// Otherwise allocates a temporary register and stores value into it.
    mir::SSARegister* resolveToRegister(ir::Value const* value);

    mir::SSARegister* nextRegister(size_t numWords = 1) {
        return nextRegistersFor(numWords, nullptr);
    }

    mir::SSARegister* nextRegistersFor(ir::Value const* value) {
        return nextRegistersFor(numWords(value->type()), value);
    }

    mir::SSARegister* nextRegistersFor(size_t numWords,
                                       ir::Value const* liveWith);

    template <mir::InstructionData T = uint64_t>
    mir::Instruction* newInst(mir::InstCode code,
                              mir::Register* dest,
                              utl::small_vector<mir::Value*> operands,
                              T instData = {},
                              size_t width = 8);

    template <mir::InstructionData T = uint64_t>
    AddNewInstResult addNewInst(mir::InstCode code,
                                mir::Register* dest,
                                utl::small_vector<mir::Value*> operands,
                                T instData = {},
                                size_t width = 8) {
        return addNewInst(code,
                          dest,
                          std::move(operands),
                          instData,
                          width,
                          currentBlock->end());
    }

    template <mir::InstructionData T = uint64_t>
    AddNewInstResult addNewInst(mir::InstCode code,
                                mir::Register* dest,
                                utl::small_vector<mir::Value*> operands,
                                T instData,
                                size_t width,
                                mir::BasicBlock::ConstIterator before);

    size_t numWords(ir::Type const* type) const {
        return utl::ceil_divide(type->size(), 8);
    }

    /// Used to calculate width of a slice when issuing multiple copy
    /// instructions for large types.
    static size_t sliceWidth(size_t numBytes, size_t index, size_t numWords) {
        if (index != numWords - 1) {
            return 8;
        }
        size_t res = numBytes % 8;
        return res == 0 ? 8 : res;
    }

    mir::Module& result;

    mir::Function* currentFunction = nullptr;
    mir::BasicBlock* currentBlock = nullptr;

    utl::hashmap<ir::Value const*, mir::Value*> valueMap;

    utl::hashmap<ir::Value const*, uint64_t> staticDataAddresses;

    ir::CompareInst const* lastEmittedCompare = nullptr;
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
}

void CodeGenContext::declareFunction(ir::Function const& function) {
    size_t const numParamRegs =
        ranges::accumulate(function.parameters(),
                           size_t(0),
                           ranges::plus{},
                           [&](auto& param) { return numWords(param.type()); });
    size_t const numRetvalRegs = numWords(function.returnType());
    auto* mirFunc = new mir::Function(&function,
                                      numParamRegs,
                                      numRetvalRegs,
                                      function.visibility());
    result.addFunction(mirFunc);
    valueMap.insert({ &function, mirFunc });
}

void CodeGenContext::genFunction(ir::Function const& function) {
    currentFunction = resolve(&function);
    for (auto& bb: function) {
        declareBasicBlock(bb);
    }
    /// Associate parameters with bottom registers.
    auto regItr = currentFunction->ssaRegisters().begin();
    for (auto& param: function.parameters()) {
        valueMap.insert({ &param, regItr.to_address() });
        std::advance(regItr, numWords(param.type()));
    }
    for (auto& bb: function) {
        genBasicBlock(bb);
    }
}

void CodeGenContext::declareBasicBlock(ir::BasicBlock const& bb) {
    auto* mirBB = new mir::BasicBlock(&bb);
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
    auto* type = allocaInst.allocatedType();
    auto* countConstant = cast<ir::IntegralConstant const*>(allocaInst.count());
    size_t count = countConstant->value().to<size_t>();
    size_t numBytes = utl::round_up(type->size() * count, 8);
    addNewInst(mir::InstCode::LIncSP,
               resolve(&allocaInst),
               { result.constant(numBytes, 2) });
}

void CodeGenContext::genInst(ir::Store const& store) {
    mir::MemoryAddress dest = computeAddress(store.address());
    mir::Value* src = resolveToRegister(store.value());
    size_t numBytes = store.value()->type()->size();
    size_t numWords = utl::ceil_divide(numBytes, 8);
    auto addrConstData = dest.constantData();
    for (size_t i = 0; i < numWords; ++i, src = src->next()) {
        addNewInst(mir::InstCode::Store,
                   nullptr,
                   { dest.addressRegister(), dest.offsetRegister(), src },
                   addrConstData,
                   sliceWidth(numBytes, i, numWords));
        addrConstData.offsetTerm += 8;
    }
}

void CodeGenContext::genInst(ir::Load const& load) {
    mir::MemoryAddress src = computeAddress(load.address());
    auto* dest = resolve(&load);
    if (!src.addressRegister()) {
        return;
    }
    size_t numBytes = load.type()->size();
    size_t numWords = utl::ceil_divide(numBytes, 8);
    auto addrConstData = src.constantData();
    for (size_t i = 0; i < numWords; ++i, dest = dest->next()) {
        addNewInst(mir::InstCode::Load,
                   dest,
                   { src.addressRegister(), src.offsetRegister() },
                   addrConstData,
                   sliceWidth(numBytes, i, numWords));
        addrConstData.offsetTerm += 8;
    }
}

void CodeGenContext::genInst(ir::ConversionInst const& inst) {
    switch (inst.conversion()) {
    case ir::Conversion::Zext:
        [[fallthrough]];
    case ir::Conversion::Trunc:
        [[fallthrough]];
    case ir::Conversion::Bitcast: {
        auto* operand = resolve(inst.operand());
        if (auto* constant = dyncast<mir::Constant*>(operand)) {
            size_t fromWidth =
                cast<ir::ArithmeticType const*>(inst.operand()->type())
                    ->bitwidth();
            size_t toWidth =
                cast<ir::ArithmeticType const*>(inst.type())->bitwidth();
            APInt value = APInt(constant->value(), fromWidth);
            value.zext(toWidth);
            valueMap.insert({ &inst,
                              result.constant(value.to<uint64_t>(),
                                              utl::ceil_divide(toWidth, 8)) });
        }
        else if (auto* undef = dyncast<mir::UndefValue*>(operand)) {
            valueMap.insert({ &inst, operand });
        }
        else {
            SC_ASSERT(isa<mir::Register>(operand), "");
            valueMap.insert({ &inst, operand });
            return;
        }
        return;
    }
    case ir::Conversion::Sext:
        [[fallthrough]];
    case ir::Conversion::Fext:
        [[fallthrough]];
    case ir::Conversion::Ftrunc:
        [[fallthrough]];
    case ir::Conversion::UtoF:
        [[fallthrough]];
    case ir::Conversion::StoF:
        [[fallthrough]];
    case ir::Conversion::FtoU:
        [[fallthrough]];
    case ir::Conversion::FtoS: {
        mir::Value* operand = resolve(inst.operand());
        auto fromBits = utl::narrow_cast<u16>(
            cast<ir::ArithmeticType const*>(inst.operand()->type())
                ->bitwidth());
        auto toBits = utl::narrow_cast<u16>(
            cast<ir::ArithmeticType const*>(inst.type())->bitwidth());
        struct ConversionData {
            mir::Conversion conv;
            u16 fromBits, toBits;
        };
        addNewInst(mir::InstCode::Conversion,
                   resolve(&inst),
                   { operand },
                   ConversionData{ inst.conversion(), fromBits, toBits },
                   inst.operand()->type()->size());
        return;
    }
    case ir::Conversion::_count:
        SC_UNREACHABLE();
    }
}

void CodeGenContext::genInst(ir::CompareInst const& cmp) {
    lastEmittedCompare = &cmp;
    auto* lhs = resolveToRegister(cmp.lhs());
    auto* rhs = resolve(cmp.rhs());
    addNewInst(mir::InstCode::Compare, nullptr, { lhs, rhs }, cmp.mode());
    addNewInst(mir::InstCode::Set, resolve(&cmp), {}, cmp.operation());
}

void CodeGenContext::genInst(ir::UnaryArithmeticInst const& inst) {
    auto* operand = resolveToRegister(inst.operand());
    addNewInst(mir::InstCode::UnaryArithmetic,
               resolve(&inst),
               { operand },
               inst.operation(),
               8);
}

void CodeGenContext::genInst(ir::ArithmeticInst const& inst) {
    auto* lhs = resolveToRegister(inst.lhs());
    auto* rhs = resolve(inst.rhs());
    /// Shift instructions only allow 8 bit literals as RHS operand.
    if (isShift(inst.operation()) && isa<mir::Constant>(rhs)) {
        rhs = result.constant(cast<mir::Constant*>(rhs)->value(), 1);
    }
    size_t size = inst.lhs()->type()->size();
    if (size < 4) {
        size = 8;
        if (auto* constant = dyncast<mir::Constant*>(rhs)) {
            rhs = result.constant(constant->value(), 8);
        }
    }
    addNewInst(mir::InstCode::Arithmetic,
               resolve(&inst),
               { lhs, rhs },
               inst.operation(),
               size);
}

void CodeGenContext::genInst(ir::Goto const& gt) {
    auto* target = resolve(gt.target());
    addNewInst(mir::InstCode::Jump, nullptr, { target });
}

void CodeGenContext::genInst(ir::Branch const& br) {
    auto condition = readCondition(br.condition());
    auto* thenTarget = resolve(br.thenTarget());
    auto* elseTarget = resolve(br.elseTarget());
    addNewInst(mir::InstCode::CondJump,
               nullptr,
               { elseTarget },
               inverse(condition));
    addNewInst(mir::InstCode::Jump, nullptr, { thenTarget });
}

void CodeGenContext::genInst(ir::Call const& call) {
    utl::small_vector<mir::Value*, 16> args;
    mir::CallInstData callData{};
    // clang-format off
    mir::InstCode const instcode = SC_MATCH (*call.function()) {
        [&](ir::Function const& func) {
            args.push_back(resolve(&func));
            return mir::InstCode::Call;
        },
        [&](ir::ForeignFunction const& func) {
            callData.extFuncAddress = {
                .slot  = static_cast<uint32_t>(func.slot()),
                .index = static_cast<uint32_t>(func.index())
            };
            return mir::InstCode::CallExt;
        },
        [&](ir::Value const& value) -> mir::InstCode {
            auto* mirVal = resolve(&value);
            args.push_back(mirVal);
            return mir::InstCode::Call;
        },
    }; // clang-format on
    for (auto* arg: call.arguments()) {
        auto* mirArg = resolve(arg);
        size_t const numWords = this->numWords(arg->type());
        for (size_t i = 0; i < numWords; ++i, mirArg = mirArg->next()) {
            args.push_back(mirArg);
        }
    }
    size_t const numDests = numWords(call.type());
    auto* dest = resolve(&call);
    auto* mirCall =
        addNewInst(instcode, nullptr, std::move(args), callData).inst;
    mirCall->setDest(dest, numDests);
    /// We set this to null because function calls clobber the CPUs compare
    /// flags
    lastEmittedCompare = nullptr;
}

void CodeGenContext::genInst(ir::Return const& ret) {
    utl::small_vector<mir::Value*, 16> args;
    auto* retval = resolve(ret.value());
    for (size_t i = 0, end = numWords(ret.value()->type()); i < end;
         ++i, retval = retval->next())
    {
        args.push_back(retval);
    }
    addNewInst(mir::InstCode::Return, nullptr, std::move(args));
}

void CodeGenContext::genInst(ir::Phi const& phi) {
    auto* dest = resolve(&phi);
    auto arguments = phi.arguments() |
                     ranges::views::transform([&](ir::ConstPhiMapping arg) {
                         return resolve(arg.value);
                     }) |
                     ToSmallVector<>;
    size_t const numBytes = phi.type()->size();
    size_t const numWords = utl::ceil_divide(numBytes, 8);
    for (size_t i = 0; i < numWords; ++i) {
        auto insertPoint = std::prev(currentBlock->end());
        while (insertPoint != currentBlock->begin() &&
               insertPoint->instcode() != mir::InstCode::Phi)
        {
            --insertPoint;
        }
        ++insertPoint;
        addNewInst(mir::InstCode::Phi,
                   dest,
                   arguments,
                   0,
                   sliceWidth(numBytes, i, numWords),
                   insertPoint);
        dest = dest->next();
        ranges::for_each(arguments, [](auto& arg) { arg = arg->next(); });
    }
}

void CodeGenContext::genInst(ir::GetElementPointer const& gep) {
    bool const allUsersAreLoadsAndStores =
        ranges::all_of(gep.users(), [&](ir::User const* user) {
            if (isa<ir::Load>(user)) {
                return true;
            }
            if (auto* store = dyncast<ir::Store const*>(user)) {
                return store->value() != &gep;
            }
            return false;
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

static std::pair<ir::Type const*, size_t> computeInnerTypeAndByteOffset(
    ir::Type const* type, std::span<size_t const> indices) {
    size_t byteOffset = 0;
    for (size_t index: indices) {
        auto* record = cast<ir::RecordType const*>(type);
        byteOffset += record->offsetAt(index);
        type = record->elementAt(index);
    }
    return { type, byteOffset };
}

template <typename R>
static R* advance(R* r, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        r = r->next();
    }
    return r;
}

static uint64_t makeWordMask(size_t leadingZeroBytes, size_t oneBytes) {
    SC_ASSERT(leadingZeroBytes + oneBytes <= 8, "");
    std::array<uint8_t, 8> mask{};
    for (size_t i = leadingZeroBytes; i < leadingZeroBytes + oneBytes; ++i) {
        mask[i] = 0xFF;
    }
    return utl::bit_cast<uint64_t>(mask);
}

void CodeGenContext::genInst(ir::ExtractValue const& extract) {
    auto* source = resolve(extract.baseValue());
    if (auto* constant = dyncast<mir::Constant*>(source)) {
        SC_UNIMPLEMENTED();
        return;
    }
    if (auto* undef = dyncast<mir::UndefValue*>(source)) {
        valueMap.insert({ &extract, undef });
        return;
    }
    mir::Register* srcreg = cast<mir::Register*>(source);
    ir::Type const* const outerType = extract.baseValue()->type();
    auto const [innerType, innerByteBegin] =
        computeInnerTypeAndByteOffset(outerType, extract.memberIndices());
    size_t const innerWordBegin = innerByteBegin / 8;
    size_t const innerByteOffset = innerByteBegin % 8;
    size_t const innerSize = innerType->size();
    srcreg = advance(srcreg, innerWordBegin);
    /// If `innerByteOffset` is 0 i.e. we don't need any bit shifts or masking,
    /// we directly associate the source register with the dest register.
    if (innerByteOffset == 0) {
        valueMap.insert({ &extract, srcreg });
        return;
    }
    SC_ASSERT(innerByteOffset + innerSize <= 8,
              "This will need even more work");
    auto* sourceShifted = nextRegister();
    auto* shiftOffset = result.constant(8 * innerByteOffset, 1);
    addNewInst(mir::InstCode::Arithmetic,
               sourceShifted,
               { srcreg, shiftOffset },
               mir::ArithmeticOperation::LShR);
    auto* sourceMask = result.constant(makeWordMask(0, innerSize), 8);
    mir::Register* dest = resolve(&extract);
    addNewInst(mir::InstCode::Arithmetic,
               dest,
               { sourceShifted, sourceMask },
               mir::ArithmeticOperation::And);
}

void CodeGenContext::genInst(ir::InsertValue const& insert) {
    auto* insertedMember = resolve(insert.insertedValue());
    auto* source = resolve(insert.baseValue());
    auto* dest = resolve(&insert);

    /// Slice the outer value like so (`x` marks parts of the inner value, `_`
    /// marks the rest of the outer value, and `outerWordCount` is the number of
    /// words of the outer value):
    /// ```
    ///        ┌─ innerByteOffset // Distance between `innerWordBegin` and
    ///        │                  // `innerByteBegin`
    ///        v
    /// [__|__|_x|xx|xx|xx|xx|xx|x_|__|__]
    ///        ^^                 ^ ^
    ///        │|                 | |
    ///        │└─ innerByteBegin └─┼─ innerByteEnd
    ///        │                    |
    ///        └── innerWordBegin   └─ innerWordEnd
    /// ```
    /// This partitions the outer value into 3 subranges:
    /// `[0, innerWordBegin)` the first words not touching the inner value.
    /// `[innerWordBegin, innerWordEnd)` the first words containing the inner
    /// value.
    /// `[innerWordEnd, outerWordCount)` the last words not touching the inner
    /// value.

    ir::Type const* const outerType = insert.type();
    auto const [innerType, innerByteBegin] =
        computeInnerTypeAndByteOffset(outerType, insert.memberIndices());

    [[maybe_unused]] size_t const innerByteEnd =
        innerByteBegin + innerType->size();
    size_t const innerWordBegin = innerByteBegin / 8;
    size_t const innerWordEnd = innerWordBegin + numWords(innerType);

    /// Copy the first full words
    dest = genCopy(dest, source, 8 * innerWordBegin);
    source = advance(source, innerWordBegin);

    /// Handle the complex middle part
    size_t const innerByteOffset = innerByteBegin % 8;
    if (innerByteOffset == 0) {
        /// If we are on a word boundary things are kind of easy.
        /// We emit copies for all full words of the inner value.
        size_t const fullWordsInner = innerType->size() / 8;
        dest = genCopy(dest, insertedMember, 8 * fullWordsInner);
        insertedMember = advance(insertedMember, fullWordsInner);
        source = advance(source, fullWordsInner);
        /// These are the bytes we hang over into the last register of the inner
        /// section.
        size_t const hungOverBytes = innerType->size() % 8;
        if (hungOverBytes != 0) {
            auto* maskedSource = nextRegister();
            auto* sourceMask =
                result.constant(~uint64_t{ 0 } << 8 * hungOverBytes, 8);
            addNewInst(mir::InstCode::Arithmetic,
                       maskedSource,
                       { source, sourceMask },
                       mir::ArithmeticOperation::And);
            auto* maskedInserted = nextRegister();
            auto* insertedMask = result.constant(~sourceMask->value(), 8);
            addNewInst(mir::InstCode::Arithmetic,
                       maskedInserted,
                       { insertedMember, insertedMask },
                       mir::ArithmeticOperation::And);
            addNewInst(mir::InstCode::Arithmetic,
                       dest,
                       { maskedSource, maskedInserted },
                       mir::ArithmeticOperation::Or);
            dest = dest->next();
            source = source->next();
        }
    }
    else {
        /// We only handle the case where we need to take care of only one word.
        SC_ASSERT(innerByteOffset + innerType->size() <= 8,
                  "Everything else is too complex for now");
        auto* shiftCount = result.constant(8 * innerByteOffset, 1);
        auto* insertedMask = result.constant(
            makeWordMask(/* leadingZeroBytes = */ innerByteOffset,
                         /* oneBytes         = */ innerType->size()),
            8);
        auto* sourceMask = result.constant(~insertedMask->value(), 8);
        auto* shiftedInsert = nextRegister();
        addNewInst(mir::InstCode::Arithmetic,
                   shiftedInsert,
                   { insertedMember, shiftCount },
                   mir::ArithmeticOperation::LShL);
        auto* maskedSource = nextRegister();
        addNewInst(mir::InstCode::Arithmetic,
                   maskedSource,
                   { source, sourceMask },
                   mir::ArithmeticOperation::And);
        auto* maskedInsert = nextRegister();
        addNewInst(mir::InstCode::Arithmetic,
                   maskedInsert,
                   { shiftedInsert, insertedMask },
                   mir::ArithmeticOperation::And);
        addNewInst(mir::InstCode::Arithmetic,
                   dest,
                   { maskedSource, maskedInsert },
                   mir::ArithmeticOperation::Or);
        dest = dest->next();
        source = source->next();
    }

    /// Copy the last full words
    dest = genCopy(dest,
                   source,
                   utl::round_up(outerType->size(), 8) - 8 * innerWordEnd);
    (void)dest;
}

void CodeGenContext::genInst(ir::Select const& select) {
    auto condition = readCondition(select.condition());
    auto* thenVal = resolve(select.thenValue());
    auto* elseVal = resolve(select.elseValue());
    size_t numBytes = select.type()->size();
    size_t numWords = utl::ceil_divide(numBytes, 8);
    auto* dest = resolve(&select);
    for (size_t i = 0; i < numWords; ++i) {
        addNewInst(mir::InstCode::Select,
                   dest,
                   { thenVal, elseVal },
                   condition,
                   sliceWidth(numBytes, i, numWords),
                   currentBlock->end());
        dest = dest->next();
        thenVal = thenVal->next();
        elseVal = elseVal->next();
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
    auto* base = resolve(gep->basePointer());
    if (auto* undef = dyncast<mir::UndefValue*>(base)) {
        return mir::MemoryAddress(nullptr, nullptr, 0, 0);
    }
    if (auto* constant = dyncast<mir::Constant*>(base)) {
        SC_UNIMPLEMENTED();
    }
    mir::Register* basereg = cast<mir::Register*>(base);
    mir::Register* dynFactor = [&]() -> mir::Register* {
        auto* constIndex =
            dyncast<ir::IntegralConstant const*>(gep->arrayIndex());
        if (constIndex && constIndex->value() == 0) {
            return nullptr;
        }
        mir::Value* arrayIndex = resolve(gep->arrayIndex());
        if (auto* regArrayIdx = dyncast<mir::Register*>(arrayIndex)) {
            return regArrayIdx;
        }
        auto* result = nextRegistersFor(1, gep);
        genCopy(result, arrayIndex, 8);
        return result;
    }();
    auto* accessedType = gep->inboundsType();
    size_t const elemSize = accessedType->size();
    auto [innerType, innerOffset] =
        computeInnerTypeAndByteOffset(accessedType, gep->memberIndices());
    return mir::MemoryAddress(basereg,
                              dynFactor,
                              utl::narrow_cast<uint32_t>(elemSize),
                              utl::narrow_cast<uint32_t>(innerOffset));
}

template <typename R>
R* CodeGenContext::genCopy(R* dest,
                           mir::Value* source,
                           size_t numBytes,
                           mir::BasicBlock::ConstIterator before,
                           mir::InstCode code,
                           uint64_t instData) {
    size_t const numWords = utl::ceil_divide(numBytes, 8);
    for (size_t i = 0; i < numWords;
         ++i, dest = dest->next(), source = source->next())
    {
        addNewInst(code,
                   dest,
                   { source },
                   instData,
                   sliceWidth(numBytes, i, numWords),
                   before);
    }
    return dest;
}

mir::CompareOperation CodeGenContext::readCondition(
    ir::Value const* condition) {
    /// If our condition is the last emitted compare operation, the compare
    /// flags are still set and we can just read them directly. We also have to
    /// check if it was emitted in the same basic block, since we only emit
    /// instructions linearly within basic blocks.
    if (condition == lastEmittedCompare &&
        currentBlock->irBasicBlock() == lastEmittedCompare->parent())
    {
        return lastEmittedCompare->operation();
    }
    /// Otherwise we have to generate a `test` instruction.
    auto* cond = resolveToRegister(condition);
    addNewInst(mir::InstCode::Test,
               nullptr,
               { cond },
               mir::CompareMode::Unsigned,
               1);
    lastEmittedCompare = nullptr;
    return mir::CompareOperation::NotEqual;
}

static size_t getOffset(void const* begin, void const* end) {
    auto* a = static_cast<char const*>(begin);
    auto* b = static_cast<char const*>(end);
    SC_ASSERT(a <= b, "");
    return static_cast<size_t>(b - a);
}

mir::Value* CodeGenContext::resolveImpl(ir::Value const* value) {
    auto itr = valueMap.find(value);
    if (itr != valueMap.end()) {
        return itr->second;
    }
    // clang-format off
    return visit(*value, utl::overload{
        [&](ir::Instruction const& inst) -> mir::Register* {
            if (isa<ir::VoidType>(inst.type())) {
                return nullptr;
            }
            auto* reg = nextRegistersFor(&inst);
            valueMap.insert({ &inst, reg });
            return reg;
        },
        [&](ir::GlobalVariable const& var) {
            uint64_t const address = [&] {
                auto itr = staticDataAddresses.find(&var);
                if (itr != staticDataAddresses.end()) {
                    return itr->second;
                }
                auto* value = var.initializer();
                size_t size = value->type()->size();
                size_t align = value->type()->align();
                auto [data, offset] = result.allocateStaticData(size, align);
                /// Callback is only executed by function pointers
                auto callback = [&, data = data, offset = offset]
                                (ir::Constant const* value, void* dest) {
                    auto* function = cast<ir::Function const*>(value);
                    result.addAddressPlaceholder(offset + getOffset(data, dest),
                                                 resolve(function));
                };
                var.initializer()->writeValueTo(data, callback);
                /// FIXME: Slot index 1 is hard coded here.
                auto address = utl::bit_cast<uint64_t>(svm::VirtualPointer{ .offset = offset, .slotIndex = 1 });
                staticDataAddresses[&var] = address;
                return address;
            }();
            auto* dest = nextRegister();
            addNewInst(mir::InstCode::Copy,
                       dest,
                       { result.constant(address, 8) });
            return dest;
        },
        [&](ir::IntegralConstant const& constant) {
            SC_ASSERT(constant.type()->bitwidth() <= 64, "");
            uint64_t value = constant.value().to<uint64_t>();
            auto* mirConst = result.constant(value, constant.type()->size());
            valueMap.insert({ &constant, mirConst });
            return mirConst;
        },
        [&](ir::FloatingPointConstant const& constant) -> mir::Value* {
            SC_ASSERT(constant.type()->bitwidth() <= 64, "");
            uint64_t value = 0;
            if (constant.value().precision() == APFloatPrec::Single) {
                value = utl::bit_cast<uint32_t>(constant.value().to<float>());
            }
            else {
                value = utl::bit_cast<uint64_t>(constant.value().to<double>());
            }
            auto* mirConst = result.constant(value, constant.type()->size());
            valueMap.insert({ &constant, mirConst });
            return mirConst;
        },
        [&](ir::NullPointerConstant const& constant) -> mir::Value* {
            auto* mirConstant = result.constant(0, 8);
            valueMap.insert({ &constant, mirConstant });
            return mirConstant;
        },
        [&](ir::RecordConstant const& value) -> mir::Value* {
            size_t numWords = this->numWords(value.type());
            utl::small_vector<uint64_t> words(numWords);
            value.writeValueTo(words.data());
            auto* reg = nextRegister(numWords);
            for (auto* dest = reg; auto word: words) {
                addNewInst(mir::InstCode::Copy,
                           dest,
                           { result.constant(word, 8) });
                dest = dest->next();
            }
            return reg;
        },
        [&](ir::UndefValue const&) -> mir::Value* {
            return result.undefValue();
        },
        [](ir::Value const& value) -> mir::Value* {
            SC_UNREACHABLE("Everything else shall be forward declared");
        }
    }); // clang-format on
}

mir::SSARegister* CodeGenContext::resolveToRegister(ir::Value const* value) {
    auto* result = resolve(value);
    if (auto* reg = dyncast<mir::SSARegister*>(result)) {
        return reg;
    }
    auto* reg = nextRegister(numWords(value->type()));
    genCopy(reg, result, value->type()->size());
    return reg;
}

mir::SSARegister* CodeGenContext::nextRegistersFor(size_t numWords,
                                                   ir::Value const* value) {
    auto* result = new mir::SSARegister();
    currentFunction->ssaRegisters().add(result);
    for (size_t i = 1; i < numWords; ++i) {
        auto* r = new mir::SSARegister();
        currentFunction->ssaRegisters().add(r);
    }
    return result;
}

template <mir::InstructionData T>
mir::Instruction* CodeGenContext::newInst(
    mir::InstCode code,
    mir::Register* dest,
    utl::small_vector<mir::Value*> operands,
    T data,
    size_t width) {
    return new mir::Instruction(code, dest, std::move(operands), data, width);
}

template <mir::InstructionData T>
AddNewInstResult CodeGenContext::addNewInst(
    mir::InstCode code,
    mir::Register* dest,
    utl::small_vector<mir::Value*> operands,
    T data,
    size_t width,
    mir::BasicBlock::ConstIterator before) {
    auto* inst = newInst(code, dest, std::move(operands), data, width);
    currentBlock->insert(before, inst);
    return { .reg = dest, .inst = inst };
}
