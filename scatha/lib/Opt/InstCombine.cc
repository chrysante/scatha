#include "Opt/Passes.h"

#include <iostream>

#include <range/v3/algorithm.hpp>
#include <utl/functional.hpp> // for ceil_divide
#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include "Common/Base.h"
#include "Common/Ranges.h"
#include "IR/Builder.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/PassRegistry.h"
#include "IR/PointerInfo.h"
#include "IR/Print.h"
#include "IR/Type.h"
#include "IR/Validate.h"
#include "Opt/AccessTree.h"
#include "Opt/Common.h"

using namespace scatha;
using namespace ir;
using namespace opt;
using namespace ranges::views;

SC_REGISTER_PASS(opt::instCombine, "instcombine", PassCategory::Simplification);

namespace {

/// Worklist for the algorithm. This ensures that no instruction appears twice
/// on the worklist and also preserves insertion order.
class Worklist {
public:
    /// Construct a worklist from \p function. Inserts all instructions from the
    /// function into the worklist
    explicit Worklist(Function& function,
                      utl::hashset<Instruction*>& eraseList):
        items(function.instructions() | TakeAddress | ToSmallVector<>),
        eraseList(eraseList) {}

    /// \Returns `true` if the worklist is empty
    bool empty() const { return index == items.size(); }

    /// Push an instruction \p inst to the back of the worklist
    /// \p inst is not added if it is already in the worklist or if it is in the
    /// erase list
    void push(Instruction* inst) {
        SC_EXPECT(inst);
        if (eraseList.contains(inst)) {
            return;
        }
        if (ranges::contains(items.begin() + index, items.end(), inst)) {
            return;
        }
        items.push_back(inst);
    }

    /// Push \p value to the list if it is an instruction
    void pushValue(Value* value) {
        if (auto* inst = dyncast<Instruction*>(value)) {
            push(inst);
        }
    }

    /// Push all users of \p value onto the worklist
    void pushUsers(Value* value) {
        for (auto* user: value->users()) {
            pushValue(user);
        }
    }

    /// Pop the first instruction off the worklist.
    /// \Returns the popped instruction
    Instruction* pop() { return items[index++]; }

private:
    size_t index = 0;
    utl::small_vector<Instruction*> items;
    utl::hashset<Instruction*>& eraseList;
};

struct InstCombineCtx {
    InstCombineCtx(Context& irCtx, Function& function):
        irCtx(irCtx), function(function), worklist(function, eraseList) {}

    bool run();

    /// \Returns replacement value if any folding occured
    /// The visit functions never update users themselves, they only return the
    /// replacement value if they find any
    Value* visitInstruction(Instruction* inst);

    Value* visitImpl(Instruction*) { return nullptr; }
    Value* visitImpl(GetElementPointer* inst);
    Value* visitImpl(ArithmeticInst* inst);
    Value* visitImpl(Load* inst);
    Value* visitImpl(ConversionInst* inst);
    Value* visitImpl(Phi* phi);
    Value* visitImpl(Select* inst);
    Value* visitImpl(CompareInst* inst);
    Value* visitImpl(UnaryArithmeticInst* inst);
    Value* visitImpl(ExtractValue* inst);
    Value* visitImpl(InsertValue* inst);

    Value* loadConstant(Load* load, Constant* base,
                        std::span<GetElementPointer* const> geps,
                        ssize_t byteOffset);
    Value* loadConstPunning(Load* load, Constant* base, ssize_t byteOffset);

    Value* extractPhiValue(ExtractValue* extractInst);
    Value* extractInsertValue(ExtractValue* extractInst);
    Value* extractConstant(ExtractValue* extractInst);

    void mergeArithmetic(ArithmeticInst* inst);
    template <ArithmeticOperation AddOp, ArithmeticOperation SubOp,
              typename ConstantType>
    void mergeAdditiveImpl(ArithmeticInst* inst, Constant* rhs,
                           ArithmeticInst* prevInst, Constant* prevRHS);
    template <ArithmeticOperation MulOp, ArithmeticOperation DivOp,
              typename ConstantType>
    void mergeMultiplicativeImpl(ArithmeticInst* inst, Constant* rhs,
                                 ArithmeticInst* prevInst, Constant* prevRHS);

    bool tryMergeNegate(ArithmeticInst* inst);

    bool isUsed(Instruction* inst) const {
        if (hasSideEffects(inst) || isa<TerminatorInst>(inst)) {
            return true;
        }
        return ranges::any_of(inst->users(), [&](auto* user) {
            return !eraseList.contains(user);
        });
    }

    void markForDeletion(Instruction* inst) {
        for (auto* op: inst->operands()) {
            worklist.pushValue(op);
        }
        eraseList.insert(inst);
    }

    void clean() {
        for (auto* inst: eraseList) {
            inst->parent()->erase(inst);
        }
        for (auto* ev: evList) {
            if (ev->userCount() == 0) {
                ev->parent()->erase(ev);
            }
        }
    }

    void replaceInst(Instruction* oldInst, Value* newValue) {
        if (oldInst == newValue) {
            return;
        }
        auto users = oldInst->users() | ToSmallVector<>;
        for (auto* user: users) {
            invalidateAccessTree(user);
            user->updateOperand(oldInst, newValue);
        }
    }

    void invalidateAccessTree(Instruction* inst) {
        if (!isa<InsertValue>(inst) && !isa<ExtractValue>(inst)) {
            return;
        }
        accessTrees.erase(inst);
        if (auto* insert = dyncast<InsertValue*>(inst)) {
            for (auto* user: insert->users()) {
                invalidateAccessTree(user);
            }
        }
    }

    AccessTree* getAccessTree(ExtractValue* inst);
    AccessTree* getAccessTree(InsertValue* inst);

    template <typename Inst>
    AccessTree* getAccessTreeCommon(Inst* inst);

    Context& irCtx;
    Function& function;
    utl::hashset<Instruction*> eraseList;
    Worklist worklist;
    /// `ExtractValue` instructions that have been inserted as missing leaves in
    /// the access trees. Will be traversed after the algorithm has run to check
    /// if they are used or can be deleted.
    utl::hashset<ExtractValue*> evList;
    bool modifiedAny = false;
    utl::hashmap<Instruction*, std::unique_ptr<AccessTree>> accessTrees;
};

} // namespace

bool opt::instCombine(Context& irCtx, Function& function) {
    InstCombineCtx ctx(irCtx, function);
    bool const result = ctx.run();
    assertInvariants(irCtx, function);
    return result;
}

bool InstCombineCtx::run() {
    while (!worklist.empty()) {
        auto* inst = worklist.pop();
        if (!isUsed(inst)) {
            markForDeletion(inst);
            continue;
        }
        auto* replacement = visitInstruction(inst);
        if (!replacement) {
            continue;
        }
        modifiedAny = true;
        worklist.pushUsers(inst);
        worklist.pushValue(replacement);
        replaceInst(inst, replacement);
        if (inst != replacement) {
            markForDeletion(inst);
        }
    }
    clean();
    return modifiedAny;
}

Value* InstCombineCtx::visitInstruction(Instruction* inst) {
    return visit(*inst, [this](auto& inst) { return visitImpl(&inst); });
}

Value* InstCombineCtx::visitImpl(GetElementPointer*) {
#if 0
    if (auto offset = inst->constantByteOffset(); offset && *offset == 0) {
        return inst->basePointer();
    }
#endif
    return nullptr;
}

static bool isConstant(Value const* value, int constant) {
    if (auto* cval = dyncast<IntegralConstant const*>(value)) {
        return cval->value() == static_cast<uint64_t>(constant);
    }
    if (auto* cval = dyncast<FloatingPointConstant const*>(value)) {
        return cval->value() == static_cast<double>(constant);
    }
    return false;
}

static bool isConstant(Value const* value, APInt const& constant) {
    if (auto* cval = dyncast<IntegralConstant const*>(value)) {
        return cval->value() == constant;
    }
    return false;
}

Value* InstCombineCtx::visitImpl(ArithmeticInst* inst) {
    auto* lhs = inst->lhs();
    auto* rhs = inst->rhs();
    /// If we have a constant operand put it on the RHS if possible.
    if (irCtx.isCommutative(inst->operation()) && isa<Constant>(lhs) &&
        !isa<Constant>(rhs))
    {
        inst->swapOperands();
        std::swap(rhs, lhs);
        /// We push the users here because other arithmetic instructions that
        /// use this check for constant right hand sides of their operands and
        /// fold if possible
        worklist.pushUsers(inst);
    }
    switch (inst->operation()) {
        /// ## Addition
    case ArithmeticOperation::Add:
        if (tryMergeNegate(inst)) {
            modifiedAny = true;
            worklist.push(inst);
            return nullptr;
        }
        [[fallthrough]];
    case ArithmeticOperation::FAdd: {
        if (isConstant(rhs, 0)) {
            return lhs;
        }
        mergeArithmetic(inst);
        break;
    }

        /// ## Subtraction
    case ArithmeticOperation::Sub:
        if (isConstant(lhs, 0)) {
            auto* neg =
                new UnaryArithmeticInst(irCtx, rhs,
                                        UnaryArithmeticOperation::Negate,
                                        "negate");
            inst->parent()->insert(inst, neg);
            return neg;
        }
        if (tryMergeNegate(inst)) {
            modifiedAny = true;
            worklist.push(inst);
            return nullptr;
        }
        [[fallthrough]];
    case ArithmeticOperation::FSub:
        if (isConstant(rhs, 0)) {
            return lhs;
        }
        if (lhs == rhs) {
            return irCtx.arithmeticConstant(0, inst->type());
        }
        mergeArithmetic(inst);
        break;

        /// ## Multiplication
    case ArithmeticOperation::Mul:
        [[fallthrough]];
    case ArithmeticOperation::FMul:
        if (isConstant(rhs, 1)) {
            return lhs;
        }
        mergeArithmetic(inst);
        break;

        /// ## Division
    case ArithmeticOperation::SDiv:
        [[fallthrough]];
    case ArithmeticOperation::UDiv:
        [[fallthrough]];
    case ArithmeticOperation::FDiv:
        if (isConstant(rhs, 0)) {
            // FIXME: Return inf for floats
            return irCtx.undef(inst->type());
        }
        if (isConstant(rhs, 1)) {
            return lhs;
        }
        if (lhs == rhs) {
            return irCtx.arithmeticConstant(1, inst->type());
        }
        mergeArithmetic(inst);
        break;

        /// ## Remainder
    case ArithmeticOperation::SRem:
        [[fallthrough]];
    case ArithmeticOperation::URem:
        if (isConstant(rhs, 0)) {
            return irCtx.undef(inst->type());
        }
        if (isConstant(rhs, 1) || lhs == rhs) {
            return irCtx.arithmeticConstant(0, inst->type());
        }
        break;

        /// ## Bitwise AND
    case ArithmeticOperation::And: {
        size_t bitwidth = cast<IntegralType const*>(rhs->type())->bitwidth();
        if (isConstant(rhs, APInt(size_t(-1), bitwidth))) {
            return lhs;
        }
        if (isConstant(lhs, APInt(size_t(-1), bitwidth))) {
            return rhs;
        }
        if (lhs == rhs) {
            return lhs;
        }
        break;
    }

        /// ## Bitwise OR
    case ArithmeticOperation::Or: {
        if (isConstant(rhs, 0)) {
            return lhs;
        }
        if (isConstant(lhs, 0)) {
            return rhs;
        }
        if (lhs == rhs) {
            return lhs;
        }
        break;
    }

    case ArithmeticOperation::XOr: {
        if (lhs == rhs) {
            return irCtx.arithmeticConstant(0, rhs->type());
        }
        break;
    }

    default:
        break;
    }
    return nullptr;
}

/// Merge sequential associative instructions where the right hand side is
/// constant. I.e. merge
/// ```
/// b = a + 1
/// c = b + 1
/// ```
/// into
/// ```
/// c = a + 2
/// ```
void InstCombineCtx::mergeArithmetic(ArithmeticInst* inst) {
    /// TODO: Handle the case where arguments are undef
    auto* rhs = dyncast<Constant*>(inst->rhs());
    if (!rhs) {
        return;
    }
    auto* prevInst = dyncast<ArithmeticInst*>(inst->lhs());
    if (!prevInst) {
        return;
    }
    auto* prevRHS = dyncast<Constant*>(prevInst->rhs());
    if (!prevRHS) {
        return;
    }
    auto const instOP = inst->operation();
    using enum ArithmeticOperation;
    if (instOP == Add || instOP == Sub) {
        mergeAdditiveImpl<Add, Sub, IntegralConstant>(inst, rhs, prevInst,
                                                      prevRHS);
    }
    else if (irCtx.associativeFloatArithmetic() &&
             (instOP == FAdd || instOP == FSub))
    {
        mergeAdditiveImpl<FAdd, FSub, FloatingPointConstant>(inst, rhs,
                                                             prevInst, prevRHS);
    }
    else if (irCtx.associativeFloatArithmetic() &&
             (instOP == FMul || instOP == FDiv))
    {
        mergeMultiplicativeImpl<FMul, FDiv, FloatingPointConstant>(inst, rhs,
                                                                   prevInst,
                                                                   prevRHS);
    }
}

template <ArithmeticOperation AddOp, ArithmeticOperation SubOp,
          typename ConstantType>
void InstCombineCtx::mergeAdditiveImpl(ArithmeticInst* inst, Constant* rhs,
                                       ArithmeticInst* prevInst,
                                       Constant* prevRHS) {
    auto a = cast<ConstantType*>(rhs)->value();
    auto b = cast<ConstantType*>(prevRHS)->value();
    Constant* newRHS = nullptr;
    auto* newLHS = prevInst->lhs();
    if (inst->operation() == AddOp) {
        if (prevInst->operation() == AddOp) {
            newRHS = irCtx.arithmeticConstant(add(a, b));
        }
        else if (prevInst->operation() == SubOp) {
            newRHS = irCtx.arithmeticConstant(sub(a, b));
        }
        else {
            return;
        }
    }
    else if (inst->operation() == SubOp) {
        if (prevInst->operation() == AddOp) {
            newRHS = irCtx.arithmeticConstant(sub(a, b));
        }
        else if (prevInst->operation() == SubOp) {
            newRHS = irCtx.arithmeticConstant(add(a, b));
        }
        else {
            return;
        }
    }
    else {
        return;
    }
    inst->setRHS(newRHS);
    inst->setLHS(newLHS);
    modifiedAny = true;
    worklist.push(inst);
    worklist.push(prevInst);
}

template <ArithmeticOperation MulOp, ArithmeticOperation DivOp,
          typename ConstantType>
void InstCombineCtx::mergeMultiplicativeImpl(ArithmeticInst* inst,
                                             Constant* rhs,
                                             ArithmeticInst* prevInst,
                                             Constant* prevRHS) {
    auto a = cast<ConstantType*>(rhs)->value();
    auto b = cast<ConstantType*>(prevRHS)->value();
    Constant* newRHS = nullptr;
    auto* newLHS = prevInst->lhs();
    if (inst->operation() == MulOp) {
        if (prevInst->operation() == MulOp) {
            newRHS = irCtx.arithmeticConstant(mul(a, b));
        }
        else if (prevInst->operation() == DivOp) {
            newRHS = irCtx.arithmeticConstant(div(a, b));
        }
        else {
            return;
        }
    }
    else if (inst->operation() == DivOp) {
        if (prevInst->operation() == MulOp) {
            newRHS = irCtx.arithmeticConstant(div(b, a));
            inst->setOperation(MulOp);
        }
        else if (prevInst->operation() == DivOp) {
            newRHS = irCtx.arithmeticConstant(mul(a, b));
        }
        else {
            return;
        }
    }
    else {
        return;
    }
    inst->setRHS(newRHS);
    inst->setLHS(newLHS);
    modifiedAny = true;
    worklist.push(inst);
    worklist.push(prevInst);
}

static Value* negatedValue(Value* value) {
    auto* unary = dyncast<UnaryArithmeticInst*>(value);
    if (!unary || unary->operation() != UnaryArithmeticOperation::Negate) {
        return nullptr;
    }
    return unary->operand();
}

/// Try to merge following patterns
///
/// `  a  + (-b) => a - b
/// `(-a) +   b  => b - a`
/// `  a  - (-b) => a + b
bool InstCombineCtx::tryMergeNegate(ArithmeticInst* inst) {
    using enum ArithmeticOperation;
    SC_EXPECT(inst->operation() == Add || inst->operation() == Sub);
    if (auto* negated = negatedValue(inst->rhs())) {
        if (inst->operation() == Add) {
            inst->setOperation(Sub);
            inst->setRHS(negated);
            return true;
        }
        else {
            inst->setOperation(Add);
            inst->setRHS(negated);
            return true;
        }
    }
    else if (auto* negated = negatedValue(inst->lhs())) {
        if (inst->operation() == Add) {
            inst->setOperation(Sub);
            inst->setLHS(inst->rhs());
            inst->setRHS(negated);
            return true;
        }
    }
    return false;
}

namespace {

struct GepInfo {
    Value* basePtr;
    ssize_t byteOffset;
    utl::small_vector<GetElementPointer*> geps;
};

} // namespace

static std::pair<Value*, ssize_t> recGepBaseAndOffsetImpl(
    Value* pointer, utl::vector<GetElementPointer*>& geps) {
    auto* gep = dyncast<GetElementPointer*>(pointer);
    if (!gep) {
        return { pointer, 0 };
    }
    if (!gep->hasConstantArrayIndex()) {
        return { nullptr, 0 };
    }
    geps.push_back(gep);
    auto [base, offset] = recGepBaseAndOffsetImpl(gep->basePointer(), geps);
    return { base, offset + *gep->constantByteOffset() };
}

static GepInfo recursiveGepBaseAndOffset(Value* pointer) {
    GepInfo result;
    auto [base, offset] = recGepBaseAndOffsetImpl(pointer, result.geps);
    result.basePtr = base;
    result.byteOffset = offset;
    return result;
}

static Constant* loadConstNoPunning(Constant* base,
                                    std::span<GetElementPointer* const> geps) {
    for (size_t i = 0; i < geps.size(); ++i) {
        ssize_t arrayIndex = geps[i]->constantArrayIndex().value();
        while (i + 1 < geps.size() &&
               geps[i]->inboundsType() == geps[i + 1]->inboundsType())
        {
            SC_ASSERT(geps[i]->memberIndices().empty(),
                      "Must be empty if the next gep accesses same type");
            arrayIndex += geps[i + 1]->constantArrayIndex().value();
            ++i;
        }
        if (auto* array = dyncast<ArrayConstant*>(base)) {
            if (array->type()->elementType() != geps[i]->inboundsType() ||
                arrayIndex < 0)
            {
                return nullptr;
            }
            base = array->elementAt((size_t)arrayIndex);
        }
        else if (arrayIndex != 0) {
            return nullptr;
        }
        for (size_t index: geps[i]->memberIndices()) {
            auto* record = dyncast<RecordConstant*>(base);
            if (!record) {
                return nullptr;
            }
            if (index >= record->numElements()) {
                return nullptr;
            }
            base = record->elementAt(index);
        }
    }
    return base;
}

Value* InstCombineCtx::loadConstPunning(Load* load, Constant* base,
                                        ssize_t byteOffset) {
    if (auto* record = dyncast<RecordConstant*>(base)) {
        auto* type = record->type();
        auto members = type->members();
        auto itr =
            ranges::find(members, byteOffset, &RecordType::Member::offset);
        if (itr == members.end()) {
            return nullptr;
        }
        auto member = *itr;
        if (member.type->size() != load->type()->size()) {
            return nullptr;
        }
        size_t index = (size_t)(itr - members.begin());
        BasicBlockBuilder builder(irCtx, load->parent());
        builder.setAddPoint(load);
        auto* extract =
            builder.add<ExtractValue>(base, std::array{ index }, "extract");
        modifiedAny = true;
        worklist.push(extract);
        return extract;
    }
    return nullptr;
}

Value* InstCombineCtx::loadConstant(Load* load, Constant* base,
                                    std::span<GetElementPointer* const> geps,
                                    ssize_t byteOffset) {
    if (auto* value = loadConstNoPunning(base, geps)) {
        return value;
    }
    return loadConstPunning(load, base, byteOffset);
}

static Value* makeValueFromConstantData(Context& ctx,
                                        std::span<uint8_t const> data,
                                        Type const* type) {
    if (auto* intType = dyncast<IntegralType const*>(type)) {
        size_t size = utl::ceil_divide(data.size(), 8);
        utl::small_vector<uint64_t> localData(size);
        std::memcpy(localData.data(), data.data(), data.size());
        APInt value(localData, intType->bitwidth());
        return ctx.intConstant(value);
    }
    return nullptr;
}

/// This case performs call devirtualization among other things
Value* InstCombineCtx::visitImpl(Load* load) {
    auto [pointer, byteOffset, geps] =
        recursiveGepBaseAndOffset(load->address());
    if (!pointer) {
        return nullptr;
    }
    auto* global = dyncast<GlobalVariable*>(pointer);
    if (!global || !global->isConst()) {
        return nullptr;
    }
    /// If the geps don't do any type punning we can direct extract the accessed
    /// constant
    if (auto* elem =
            loadConstant(load, global->initializer(), geps, byteOffset))
    {
        if (elem->type() == load->type()) {
            return elem;
        }
        if (elem->type()->size() == load->type()->size()) {
            auto* conv = new ConversionInst(elem, load->type(),
                                            Conversion::Bitcast,
                                            std::string(load->name()));
            load->parent()->insert(load, conv);
            return conv;
        }
        // FIXME: These cases are unimplemented
        if (elem->type()->size() > load->type()->size()) {
            return nullptr;
        }
        else {
            return nullptr;
        }
    }
    /// Otherwise we write the constant data to a buffer and construct a new
    /// constant from that
    else {
        auto* init = global->initializer();
        utl::small_vector<uint8_t> data(init->type()->size());
        bool haveFuncPtrs = false;
        init->writeValueTo(data.data(), [&](Constant const* value,
                                            [[maybe_unused]] void* dest) {
            haveFuncPtrs |= isa<Function>(value);
        });
        /// We cannot evaluate function pointers this way though
        if (haveFuncPtrs) {
            return nullptr;
        }
        std::span subregion(data.data() + byteOffset, load->type()->size());
        return makeValueFromConstantData(irCtx, subregion, load->type());
    }
}

Value* InstCombineCtx::visitImpl(ConversionInst* inst) {
    using enum Conversion;
    switch (inst->conversion()) {
    case Zext:
        return nullptr;
    case Sext:
        return nullptr;
    case Trunc:
        return nullptr;
    case Fext:
        return nullptr;
    case Ftrunc:
        return nullptr;
    case UtoF:
        return nullptr;
    case StoF:
        return nullptr;
    case FtoU:
        return nullptr;
    case FtoS:
        return nullptr;
    case Bitcast: {
        if (auto* conv = dyncast<ConversionInst*>(inst->operand())) {
            if (conv->conversion() == Bitcast) {
                inst->setOperand(conv->operand());
                return inst;
            }
        }
        else if (auto* load = dyncast<Load*>(inst->operand())) {
            BasicBlockBuilder builder(irCtx, load->parent());
            return builder.insert<Load>(load, load->address(), inst->type(),
                                        std::string(inst->name()));
        }
        return nullptr;
    }
    case _count:
        SC_UNREACHABLE();
    }
}

Value* InstCombineCtx::visitImpl(Phi* phi) {
    Value* const first = phi->operands().front();
    bool const allEqual =
        ranges::all_of(phi->operands(), [&](auto* op) { return op == first; });
    if (allEqual) {
        return first;
    }
    return nullptr;
}

/// Returns `i1` type if \p type is `i1`. Otherwise returns `nullptr`
static ir::IntegralType const* getBoolType(ir::Type const* type) {
    auto* intType = dyncast<IntegralType const*>(type);
    if (intType && intType->bitwidth() == 1) {
        return intType;
    }
    return nullptr;
}

Value* InstCombineCtx::visitImpl(Select* inst) {
    /// If we have a constant condition we return the constantly selected value
    if (auto* constant = dyncast<IntegralConstant*>(inst->condition())) {
        SC_ASSERT(constant->value().bitwidth() == 1, "Must be bool");
        return constant->value().to<bool>() ? inst->thenValue() :
                                              inst->elseValue();
    }
    /// Replace instructions of the form `%2 = select i1 %0, <type> %1, <type>
    /// %1`  with the value `%1`
    if (inst->thenValue() == inst->elseValue()) {
        return inst->thenValue();
    }
    /// If we select between two bools, we want to replace the select by either
    /// the condition or the inverse of the condition
    if (auto* boolType = getBoolType(inst->type())) {
        SC_ASSERT(getBoolType(inst->thenValue()->type()), "");
        SC_ASSERT(getBoolType(inst->elseValue()->type()), "");
        auto* thenVal = dyncast<IntegralConstant*>(inst->thenValue());
        auto* elseVal = dyncast<IntegralConstant*>(inst->elseValue());
        if (!thenVal || !elseVal) {
            return nullptr;
        }
        if (thenVal->value().to<bool>()) {
            SC_ASSERT(!elseVal->value().to<bool>(),
                      "Can't be the same, we checked that case earlier");
            return inst->condition();
        }
        else {
            SC_ASSERT(elseVal->value().to<bool>(),
                      "Can't be the same, see case above");
            auto* lnt =
                new UnaryArithmeticInst(irCtx, inst->condition(),
                                        UnaryArithmeticOperation::LogicalNot,
                                        "select.lnt");
            inst->parent()->insert(inst, lnt);
            return lnt;
        }
    }
    return nullptr;
}

static CompareOperation operationForReversedOperands(CompareOperation op) {
    using enum CompareOperation;
    switch (op) {
    case Less:
        return Greater;
    case LessEq:
        return GreaterEq;
    case Greater:
        return Less;
    case GreaterEq:
        return LessEq;
    case Equal:
        return Equal;
    case NotEqual:
        return NotEqual;
    }
}

namespace {

enum class StaticCompareResult { Indeterminate, Equal, NotEqual };

} // namespace

static bool allocaMayAlias(Alloca const* inst, Value const* other) {
    SC_EXPECT(inst != other);
    /// Two alloca instructions can never alias each other
    if (isa<Alloca>(other)) {
        return false;
    }
    /// Automatic and dynamic allocations cannot alias
    if (isBuiltinAlloc(other)) {
        return false;
    }
    return true;
}

static bool dynAllocMayAlias(Call const* inst, Value const* other) {
    SC_EXPECT(inst != other);
    /// Two dynamic allocations can never alias each other
    if (isBuiltinAlloc(other)) {
        return false;
    }
    /// Automatic and dynamic allocations cannot alias
    if (isa<Alloca>(other)) {
        return false;
    }
    return true;
}

static bool mayAlias(Value const* A, Value const* B) {
    SC_EXPECT(A != B);
    if (auto* allocaInst = dyncast<Alloca const*>(A)) {
        return allocaMayAlias(allocaInst, B);
    }
    if (auto* allocaInst = dyncast<Alloca const*>(B)) {
        return allocaMayAlias(allocaInst, A);
    }
    if (auto* alloc = dyncast<Call const*>(A); isBuiltinAlloc(alloc)) {
        return dynAllocMayAlias(alloc, B);
    }
    if (auto* alloc = dyncast<Call const*>(B); isBuiltinAlloc(alloc)) {
        return dynAllocMayAlias(alloc, A);
    }
    return true;
}

/// Trys to determine if the pointers \p lhs and \p rhs are always equal or
/// always unequal \pre \p lhs and \p rhs must be of type `ptr`
static StaticCompareResult pointerStaticCompare(Value const* lhs,
                                                Value const* rhs) {
    SC_EXPECT(isa<PointerType>(lhs->type()));
    SC_EXPECT(isa<PointerType>(rhs->type()));
    using enum StaticCompareResult;
    if (lhs == rhs) {
        return Equal;
    }
    auto* lhsInfo = lhs->pointerInfo();
    auto* rhsInfo = rhs->pointerInfo();
    if (!lhsInfo || !lhsInfo->hasProvAndStaticOffset() || !rhsInfo ||
        !rhsInfo->hasProvAndStaticOffset())
    {
        return Indeterminate;
    }
    if (!mayAlias(lhsInfo->provenance(), rhsInfo->provenance())) {
        return NotEqual;
    }
    if (lhsInfo->guaranteedNotNull() && isa<NullPointerConstant>(rhs)) {
        return NotEqual;
    }
    return Indeterminate;
}

Value* InstCombineCtx::visitImpl(CompareInst* inst) {
    using enum CompareOperation;
    auto* lhs = inst->lhs();
    auto* rhs = inst->rhs();
    /// If we have a constant operand put it on the RHS
    if (isa<Constant>(lhs) && !isa<Constant>(rhs)) {
        inst->setOperation(operationForReversedOperands(inst->operation()));
        inst->swapOperands();
        std::swap(rhs, lhs);
        /// We push the users here because other arithmetic instructions that
        /// use this check for constant right hand sides of their operands and
        /// fold if possible
        worklist.pushUsers(inst);
    }
    switch (inst->operation()) {
    case Equal:
        if (lhs == rhs) {
            return irCtx.boolConstant(true);
        }
        if (isa<PointerType>(lhs->type())) {
            using enum StaticCompareResult;
            auto cmp = pointerStaticCompare(lhs, rhs);
            if (cmp == Equal) {
                return irCtx.boolConstant(true);
            }
            if (cmp == NotEqual) {
                return irCtx.boolConstant(false);
            }
        }
        return nullptr;
    case NotEqual:
        if (lhs == rhs) {
            return irCtx.boolConstant(false);
        }
        if (isa<PointerType>(lhs->type())) {
            using enum StaticCompareResult;
            auto cmp = pointerStaticCompare(lhs, rhs);
            if (cmp == Equal) {
                return irCtx.boolConstant(false);
            }
            if (cmp == NotEqual) {
                return irCtx.boolConstant(true);
            }
        }
        return nullptr;
    default:
        return nullptr;
    }
    return nullptr;
}

Value* InstCombineCtx::visitImpl(UnaryArithmeticInst* inst) {
    using enum UnaryArithmeticOperation;
    switch (inst->operation()) {
    case BitwiseNot:
        return nullptr;

    case LogicalNot: {
        /// If we have a logical not of a compare instruction, we either rewrite
        /// the compare to its inverse operation or generate a new compare
        /// instruction with the inverse operation
        auto* compare = dyncast<CompareInst*>(inst->operand());
        if (!compare) {
            return nullptr;
        }
        if (compare->userCount() == 1) {
            compare->setOperation(inverse(compare->operation()));
            return compare;
        }
        auto* newCompare =
            new CompareInst(irCtx, compare->lhs(), compare->rhs(),
                            compare->mode(), inverse(compare->operation()),
                            utl::strcat(compare->name(), ".inv"));
        inst->parent()->insert(inst, newCompare);
        return newCompare;
    }
    case Negate:
        return nullptr;

    case _count:
        SC_UNREACHABLE();
    };
}

static Value* stitchExtractedValue(Context& irCtx, ExtractValue* extractInst,
                                   AccessTree const* accessTree) {
    auto* type = cast<RecordType const*>(accessTree->type());
    Value* base = [&]() -> Constant* {
        if (!accessTree->hasConstantChildren()) {
            return irCtx.undef(type);
        }
        auto elems = accessTree->children() |
                     transform([&](AccessTree const* node) -> Constant* {
            if (auto* constant = dyncast<Constant*>(node->value())) {
                return constant;
            }
            return irCtx.undef(node->type());
        }) | ToSmallVector<>;
        return irCtx.recordConstant(elems, type);
    }();
    for (auto* node: accessTree->children()) {
        auto* value = node->value();
        if (isa<Constant>(value)) {
            continue;
        }
        size_t index = node->index().value();
        auto* insert =
            new InsertValue(base, value, std::array{ index },
                            utl::strcat(extractInst->name(), ".", index));
        extractInst->parent()->insert(extractInst, insert);
        base = insert;
    }
    return base;
}

Value* InstCombineCtx::extractPhiValue(ExtractValue* extractInst) {
    /// If we extract from a phi node and the phi node has no other users, we
    /// perform the extract in each of the predecessors and phi them together
    auto* phi = dyncast<Phi*>(extractInst->baseValue());
    if (!phi || phi->users().size() > 1) {
        return nullptr;
    }
    utl::small_vector<PhiMapping> newPhiArgs;
    for (auto [pred, arg]: phi->arguments()) {
        auto* newExtract = new ExtractValue(arg, extractInst->memberIndices(),
                                            std::string(extractInst->name()));
        pred->insert(pred->terminator(), newExtract);
        worklist.push(newExtract);
        newPhiArgs.push_back({ pred, newExtract });
    }
    auto* newPhi = new Phi(newPhiArgs, std::string(extractInst->name()));
    /// We add the new phi node to the block of the phi node we extracted
    /// from
    phi->parent()->insertPhi(newPhi);
    return newPhi;
}

Value* InstCombineCtx::extractInsertValue(ExtractValue* extractInst) {
    /// If we extract from a structure that has been build up with
    /// `insert_value` instructions, we check every `insert_value` for a match
    /// of indices
    Value* insertBase = nullptr; /// To be used later
    for (auto* insertInst = dyncast<InsertValue*>(extractInst->baseValue());
         insertInst != nullptr;
         insertInst = dyncast<InsertValue*>(insertInst->baseValue()))
    {
        insertBase = insertInst->baseValue();
        if (ranges::equal(extractInst->memberIndices(),
                          insertInst->memberIndices()))
        {
            return insertInst->insertedValue();
        }
    }
    if (!isa<InsertValue>(extractInst->baseValue())) {
        return nullptr;
    }
    auto* accessTree = getAccessTree(extractInst);
    auto indices = extractInst->memberIndices();
    size_t i = 0;
    for (; i < indices.size() && accessTree->hasChildren(); ++i) {
        accessTree = accessTree->childAt(indices[i]);
    }
    if (i < indices.size()) {
        auto* base = accessTree->value();
        SC_ASSERT(base, "");
        auto newIndices = indices | ranges::views::drop(i) |
                          ranges::to<utl::small_vector<size_t>>;
        auto* newExtr = new ExtractValue(base, newIndices,
                                         std::string(extractInst->name()));
        extractInst->parent()->insert(extractInst, newExtr);
        return newExtr;
    }
    if (accessTree->hasChildren()) {
        /// We have to stitch the value together from the child nodes
        return stitchExtractedValue(irCtx, extractInst, accessTree);
    }
    else if (auto* base = accessTree->value()) {
        /// We can directly access the value
        return base;
    }
    else {
        SC_ASSERT(insertBase, "");
        auto* newExtr = new ExtractValue(insertBase,
                                         extractInst->memberIndices(),
                                         std::string(extractInst->name()));
        extractInst->parent()->insert(extractInst, newExtr);
        return newExtr;
    }
    return nullptr;
}

Value* InstCombineCtx::extractConstant(ExtractValue* extractInst) {
    auto* base = dyncast<Constant*>(extractInst->baseValue());
    if (!base) {
        return nullptr;
    }
    for (size_t index: extractInst->memberIndices()) {
        auto* record = dyncast<RecordConstant*>(base);
        if (!record || index >= record->numElements()) {
            return nullptr;
        }
        base = record->elementAt(index);
    }
    return base;
}

Value* InstCombineCtx::visitImpl(ExtractValue* extractInst) {
    /// Extracting from `undef` results in `undef`
    if (isa<UndefValue>(extractInst->baseValue())) {
        return irCtx.undef(extractInst->type());
    }
    if (auto* value = extractPhiValue(extractInst)) {
        return value;
    }
    if (auto* value = extractInsertValue(extractInst)) {
        return value;
    }
    if (auto* value = extractConstant(extractInst)) {
        return value;
    }
    return nullptr;
}

static void gatherMostUsedBase(utl::hashmap<Value*, size_t>& baseCount,
                               AccessTree* leaf,
                               std::span<size_t const> leafIndices) {
    if (!leaf->value()) {
        return;
    }
    auto* ev = dyncast<ExtractValue*>(leaf->value());
    if (!ev) {
        return;
    }
    if (!ranges::equal(ev->memberIndices(), leafIndices)) {
        return;
    }
    ++baseCount[ev->baseValue()];
}

static Value* maxElement(utl::hashmap<Value*, size_t> const& baseCount) {
    if (baseCount.empty()) {
        return nullptr;
    }
    return ranges::max_element(baseCount, ranges::less{}, [](auto& p) {
        return p.second;
    })->first;
}

static Value* mostUsedChildrenBase(AccessTree* node) {
    utl::hashmap<Value*, size_t> baseCount;
    for (auto [index, child]: node->children() | ranges::views::enumerate) {
        gatherMostUsedBase(baseCount, child, std::array{ index });
    }
    return maxElement(baseCount);
}

static Value* mostUsedLeavesBase(AccessTree* node) {
    utl::hashmap<Value*, size_t> baseCount;
    node->leafWalk([&](AccessTree* leaf, std::span<size_t const> leafIndices) {
        gatherMostUsedBase(baseCount, leaf, leafIndices);
    });
    return maxElement(baseCount);
}

static std::pair<Value*, utl::small_vector<UniquePtr<InsertValue>>>
    newLeavesInserts(
        AccessTree* node, ir::Context& irCtx,
        utl::hashmap<std::pair<Value*, Value*>, InsertValue*> const& ivMap) {
    utl::small_vector<UniquePtr<InsertValue>> result;
    auto* maxValue = mostUsedLeavesBase(node);
    auto* baseValue = maxValue ? maxValue : irCtx.undef(node->type());
    node->leafWalk([&](AccessTree* leaf, std::span<size_t const> leafIndices) {
        auto* ev = dyncast<ExtractValue*>(leaf->value());
        if (ev && ranges::equal(ev->memberIndices(), leafIndices) &&
            ev->baseValue() == maxValue)
        {
            return;
        }
        auto* ins = leaf->value();
        auto itr = ivMap.find({ baseValue, ins });
        if (itr != ivMap.end()) {
            auto* iv = itr->second;
            if (ranges::equal(iv->memberIndices(), leafIndices)) {
                baseValue = iv;
                return;
            }
        }
        result.push_back(
            allocate<InsertValue>(baseValue, ins, leafIndices, "iv"));
        baseValue = result.back().get();
    });
    return { baseValue, std::move(result) };
}

static std::pair<Value*, utl::small_vector<UniquePtr<InsertValue>>>
    newChildrenInserts(
        AccessTree* node, ir::Context& irCtx,
        utl::hashmap<std::pair<Value*, Value*>, InsertValue*> const& ivMap) {
    utl::small_vector<UniquePtr<InsertValue>> result;
    auto* maxValue = mostUsedChildrenBase(node);
    auto* baseValue = maxValue ? maxValue : irCtx.undef(node->type());

    for (size_t index = 0; auto child: node->children()) {
        std::array const indices{ index++ };
        auto* ev = dyncast<ExtractValue*>(child->value());
        if (ev && ranges::equal(ev->memberIndices(), indices) &&
            ev->baseValue() == maxValue)
        {
            continue;
        }
        auto* ins = child->value();
        auto itr = ivMap.find({ baseValue, ins });
        if (itr != ivMap.end()) {
            auto* iv = itr->second;
            if (ranges::equal(iv->memberIndices(),
                              ranges::views::single(index)))
            {
                baseValue = iv;
                continue;
            }
        }
        result.push_back(allocate<InsertValue>(baseValue, ins, indices, "iv"));
        baseValue = result.back().get();
    }
    return { baseValue, std::move(result) };
}

static void mergeInserts(utl::vector<UniquePtr<InsertValue>>& inserts,
                         std::span<UniquePtr<InsertValue>> chosenInserts,
                         std::span<UniquePtr<InsertValue> const> otherInserts) {
    inserts.insert(inserts.end(), std::move_iterator(chosenInserts.begin()),
                   std::move_iterator(chosenInserts.end()));
    for (auto& inst: otherInserts | ranges::views::reverse) {
        inst->clearOperands();
    }
}

/// The IV map maps pairs of `(baseValue(), insertedValue())` to the
/// corresponding `insert_value` instruction
static utl::hashmap<std::pair<Value*, Value*>, InsertValue*> gatherIVMap(
    InsertValue* inst) {
    utl::hashmap<std::pair<Value*, Value*>, InsertValue*> result;
    auto search = [&](Value* value, auto& search) {
        auto* iv = dyncast<InsertValue*>(value);
        if (!iv) {
            return;
        }
        auto* base = iv->baseValue();
        auto* ins = iv->insertedValue();
        result.insert({ std::pair{ base, ins }, iv });
        search(base, search);
        search(ins, search);
    };
    search(inst, search);
    return result;
}

Value* InstCombineCtx::visitImpl(InsertValue* insertInst) {
    if (auto* undefInsValue = dyncast<UndefValue*>(insertInst->insertedValue()))
    {
        return insertInst->baseValue();
    }
    if (ranges::all_of(insertInst->users(), [](Instruction* user) {
        return isa<InsertValue>(user) || isa<ExtractValue>(user);
    }))
    {
        return nullptr;
    }
    auto* root = getAccessTree(insertInst);
    auto const ivMap = gatherIVMap(insertInst);
    utl::small_vector<UniquePtr<InsertValue>> inserts;
    root->postOrderWalk([&](AccessTree* node, std::span<size_t const> indices) {
        if (node->children().empty()) {
            /// Create `extract_value` instructions for all nodes that have no
            /// associated value
            if (!node->value()) {
                auto* ev = new ExtractValue(root->value(), indices, "ev");
                insertInst->parent()->insert(insertInst, ev);
                node->setValue(ev);
                evList.insert(ev);
            }
            return;
        }
        auto [leavesBase, leavesInserts] = newLeavesInserts(node, irCtx, ivMap);
        auto [childrenBase, childrenInserts] =
            newChildrenInserts(node, irCtx, ivMap);

        if (childrenInserts.size() < leavesInserts.size()) {
            mergeInserts(inserts, childrenInserts, leavesInserts);
            node->setValue(childrenBase);
        }
        else {
            mergeInserts(inserts, leavesInserts, childrenInserts);
            node->setValue(leavesBase);
        }
    });
    for (auto& insert: inserts) {
        worklist.push(insert.get());
        insertInst->parent()->insert(insertInst, insert.release());
    }
    auto* newValue = root->value();
    if (newValue == insertInst) {
        return nullptr;
    }
    worklist.pushValue(insertInst->baseValue());
    worklist.pushValue(insertInst->insertedValue());
    if (auto* newInsert = dyncast<InsertValue*>(newValue)) {
        worklist.pushValue(newInsert->baseValue());
        worklist.pushValue(newInsert->insertedValue());
    }
    return newValue;
}

AccessTree* InstCombineCtx::getAccessTree(ExtractValue* inst) {
    return getAccessTreeCommon(inst);
}

AccessTree* InstCombineCtx::getAccessTree(InsertValue* inst) {
    auto* root = getAccessTreeCommon(inst);
    auto* node = root;
    for (size_t index: inst->memberIndices()) {
        node->fanOut();
        node = node->childAt(index);
    }
    node->setValue(inst->insertedValue());
    return root;
}

template <typename Inst>
AccessTree* InstCombineCtx::getAccessTreeCommon(Inst* inst) {
    if (auto itr = accessTrees.find(inst); itr != accessTrees.end()) {
        return itr->second.get();
    }
    // clang-format off
    auto treeOwner = visit(*inst->baseValue(), utl::overload{
        [&](ExtractValue& evBase) {
            return getAccessTree(&evBase)->clone();
        },
        [&](InsertValue& ivBase) {
            return getAccessTree(&ivBase)->clone();
        },
        [&](Value& base) {
            auto tree = std::make_unique<AccessTree>(base.type());
            tree->setValue(&base);
            return tree;
        }
    }); // clang-format on
    auto* root = treeOwner.get();
    accessTrees.insert({ inst, std::move(treeOwner) });
    return root;
}
