#include "Opt/Passes.h"

#include <iostream>

#include <range/v3/algorithm.hpp>
#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include "Common/Base.h"
#include "Common/Ranges.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Print.h"
#include "IR/Type.h"
#include "IR/Validate.h"
#include "Opt/AccessTree.h"
#include "Opt/Common.h"
#include "Opt/PassRegistry.h"

using namespace scatha;
using namespace ir;
using namespace opt;

SC_REGISTER_PASS(opt::instCombine, "instcombine");

namespace {

using AccessTreeNode = opt::AccessTree<Value*>;

struct InstCombineCtx {
    InstCombineCtx(Context& irCtx, Function& function):
        irCtx(irCtx), function(function) {}

    bool run();

    /// \Returns Replacement value if possible
    Value* visitInstruction(Instruction* inst);

    Value* visitImpl(Instruction* inst) { return nullptr; }
    Value* visitImpl(ArithmeticInst* inst);
    Value* visitImpl(Phi* phi);
    Value* visitImpl(ExtractValue* inst);
    Value* visitImpl(InsertValue* inst);

    void mergeArithmetic(ArithmeticInst* inst);
    template <ArithmeticOperation AddOp,
              ArithmeticOperation SubOp,
              typename ConstantType>
    void mergeAdditiveImpl(ArithmeticInst* inst,
                           Constant* rhs,
                           ArithmeticInst* prevInst,
                           Constant* prevRHS);
    template <ArithmeticOperation MulOp,
              ArithmeticOperation DivOp,
              typename ConstantType>
    void mergeMultiplicativeImpl(ArithmeticInst* inst,
                                 Constant* rhs,
                                 ArithmeticInst* prevInst,
                                 Constant* prevRHS);

    bool tryMergeNegate(ArithmeticInst* inst);

    void push(Instruction* inst) {
        if (!eraseList.contains(inst)) {
            worklist.insert(inst);
        }
    }

    void pushIfInst(Value* value) {
        if (auto* inst = dyncast<Instruction*>(value)) {
            push(inst);
        }
    }

    void pushUsers(Instruction* inst) {
        for (auto* user: inst->users()) {
            push(user);
        }
    }

    void pushUsers(Value* value) {
        for (auto* user: value->users()) {
            pushIfInst(user);
        }
    }

    bool isUsed(Instruction* inst) const {
        if (hasSideEffects(inst)) {
            return true;
        }
        return ranges::any_of(inst->users(), [&](auto* user) {
            return !eraseList.contains(user);
        });
    }

    void markForDeletion(Instruction* inst) {
        for (auto* op: inst->operands()) {
            pushIfInst(op);
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

    AccessTreeNode* getAccessTree(ExtractValue* inst);
    AccessTreeNode* getAccessTree(InsertValue* inst);

    template <typename Inst>
    AccessTreeNode* getAccessTreeCommon(Inst* inst);

    Context& irCtx;
    Function& function;
    utl::hashset<Instruction*> worklist;
    utl::hashset<Instruction*> eraseList;
    /// `ExtractValue` instructions that have been inserted as missing leaves in
    /// the access trees. Will be traversed after the algorithm has run to check
    /// if they are used or can be deleted.
    utl::hashset<ExtractValue*> evList;
    bool modifiedAny = false;
    utl::hashmap<Instruction*, std::unique_ptr<AccessTreeNode>> accessTrees;
};

} // namespace

bool opt::instCombine(Context& irCtx, Function& function) {
    InstCombineCtx ctx(irCtx, function);
    bool const result = ctx.run();
    assertInvariants(irCtx, function);
    return result;
}

bool InstCombineCtx::run() {
    worklist = function.instructions() | TakeAddress |
               ranges::to<utl::hashset<Instruction*>>;
    while (!worklist.empty()) {
        auto itr = worklist.begin();
        Instruction* inst = *itr;
        worklist.erase(itr);
        if (!isUsed(inst)) {
            markForDeletion(inst);
            continue;
        }
        auto* replacement = visitInstruction(inst);
        if (!replacement) {
            continue;
        }
        modifiedAny = true;
        pushUsers(inst);
        pushIfInst(replacement);
        replaceInst(inst, replacement);
        markForDeletion(inst);
    }
    clean();
    return modifiedAny;
}

Value* InstCombineCtx::visitInstruction(Instruction* inst) {
    return visit(*inst, [this](auto& inst) { return visitImpl(&inst); });
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
        pushUsers(inst);
    }
    switch (inst->operation()) {
        /// ## Addition
    case ArithmeticOperation::Add:
        if (tryMergeNegate(inst)) {
            modifiedAny = true;
            push(inst);
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
                new UnaryArithmeticInst(irCtx,
                                        rhs,
                                        UnaryArithmeticOperation::Negate,
                                        "negate");
            inst->parent()->insert(inst, neg);
            return neg;
        }
        if (tryMergeNegate(inst)) {
            modifiedAny = true;
            push(inst);
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
        if (isConstant(rhs, 1)) {
            return irCtx.arithmeticConstant(0, inst->type());
        }
        if (lhs == rhs) {
            return lhs;
        }
        break;

        /// ## Bitwise AND
    case ArithmeticOperation::And: {
        // TODO: Replace complex expression by APInt::max(<bitwitdh>)
        if (isConstant(rhs,
                       APInt(-1,
                             cast<IntegralType const*>(rhs->type())
                                 ->bitwidth())))
        {
            return lhs;
        }
        // TODO: mergeArithmetic(inst);
        break;
    }

        /// ## Bitwise OR
    case ArithmeticOperation::Or: {
        if (isConstant(rhs, 0)) {
            return lhs;
        }
        // TODO: mergeArithmetic(inst);
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
        mergeAdditiveImpl<Add, Sub, IntegralConstant>(inst,
                                                      rhs,
                                                      prevInst,
                                                      prevRHS);
    }
    else if (irCtx.associativeFloatArithmetic() &&
             (instOP == FAdd || instOP == FSub))
    {
        mergeAdditiveImpl<FAdd, FSub, FloatingPointConstant>(inst,
                                                             rhs,
                                                             prevInst,
                                                             prevRHS);
    }
    else if (irCtx.associativeFloatArithmetic() &&
             (instOP == FMul || instOP == FDiv))
    {
        mergeMultiplicativeImpl<FMul, FDiv, FloatingPointConstant>(inst,
                                                                   rhs,
                                                                   prevInst,
                                                                   prevRHS);
    }
}

template <ArithmeticOperation AddOp,
          ArithmeticOperation SubOp,
          typename ConstantType>
void InstCombineCtx::mergeAdditiveImpl(ArithmeticInst* inst,
                                       Constant* rhs,
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
            newRHS = irCtx.arithmeticConstant(sub(b, a));
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
    push(inst);
    push(prevInst);
}

template <ArithmeticOperation MulOp,
          ArithmeticOperation DivOp,
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
    push(inst);
    push(prevInst);
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
    SC_ASSERT(inst->operation() == ArithmeticOperation::Add ||
                  inst->operation() == ArithmeticOperation::Sub,
              "");
    if (auto* negated = negatedValue(inst->rhs())) {
        if (inst->operation() == ArithmeticOperation::Add) {
            inst->setOperation(ArithmeticOperation::Sub);
            inst->setRHS(negated);
            return true;
        }
        else {
            inst->setOperation(ArithmeticOperation::Add);
            inst->setRHS(negated);
            return true;
        }
    }
    else if (auto* negated = negatedValue(inst->lhs())) {
        if (inst->operation() == ArithmeticOperation::Add) {
            inst->setOperation(ArithmeticOperation::Sub);
            inst->setLHS(inst->rhs());
            inst->setRHS(negated);
            return true;
        }
    }
    return false;
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

static void print(auto* inst, AccessTreeNode const* tree) {
    std::cout << *inst << ":\n";
    tree->print(
        std::cout,
        [](auto* v) { return ir::toString(v); },
        1);
    std::cout << "\n";
}

Value* InstCombineCtx::visitImpl(ExtractValue* extractInst) {
    /// Extracting from `undef` results in `undef`
    if (isa<UndefValue>(extractInst->baseValue())) {
        return irCtx.undef(extractInst->type());
    }
    /// If we extract from a phi node and the phi node has no other users, we
    /// perform the extract in each of the predecessors and phi them together
    if (auto* phi = dyncast<Phi*>(extractInst->baseValue())) {
        if (phi->users().size() > 1) {
            return nullptr;
        }
        utl::small_vector<PhiMapping> newPhiArgs;
        for (auto [pred, arg]: phi->arguments()) {
            auto* newExtract =
                new ExtractValue(arg,
                                 extractInst->memberIndices(),
                                 std::string(extractInst->name()));
            pred->insert(pred->terminator(), newExtract);
            push(newExtract);
            newPhiArgs.push_back({ pred, newExtract });
        }
        auto* newPhi = new Phi(newPhiArgs, std::string(extractInst->name()));
        /// We add the new phi node to the block of the phi node we extracted
        /// from
        phi->parent()->insertPhi(newPhi);
        return newPhi;
    }
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
        auto* base = accessTree->payload();
        SC_ASSERT(base, "");
        auto newIndices = indices | ranges::views::drop(i) |
                          ranges::to<utl::small_vector<size_t>>;
        auto* newExtr = new ExtractValue(base,
                                         newIndices,
                                         std::string(extractInst->name()));
        extractInst->parent()->insert(extractInst, newExtr);
        return newExtr;
    }
    if (!accessTree->hasChildren()) {
        if (auto* base = accessTree->payload()) {
            return base;
        }
        SC_ASSERT(insertBase, "");
        auto* newExtr = new ExtractValue(insertBase,
                                         extractInst->memberIndices(),
                                         std::string(extractInst->name()));
        extractInst->parent()->insert(extractInst, newExtr);
        return newExtr;
    }
    else {
        SC_UNIMPLEMENTED();
    }
    return nullptr;
}

void printAT(AccessTree<Value*> const* node) {
    node->print(std::cout, [](Value* value) { return toString(value); });
}

static void gatherMostUsedBase(utl::hashmap<Value*, size_t>& baseCount,
                               AccessTreeNode* leaf,
                               std::span<size_t const> leafIndices) {
    if (!leaf->payload()) {
        return;
    }
    auto* ev = dyncast<ExtractValue*>(leaf->payload());
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
    return ranges::max_element(baseCount,
                               ranges::less{},
                               [](auto& p) { return p.second; })
        ->first;
}

static Value* mostUsedChildrenBase(AccessTreeNode* node) {
    utl::hashmap<Value*, size_t> baseCount;
    for (auto [index, child]: node->children() | ranges::views::enumerate) {
        gatherMostUsedBase(baseCount, child, std::array{ index });
    }
    return maxElement(baseCount);
}

static Value* mostUsedLeavesBase(AccessTreeNode* node) {
    utl::hashmap<Value*, size_t> baseCount;
    node->leafWalk(
        [&](AccessTreeNode* leaf, std::span<size_t const> leafIndices) {
        gatherMostUsedBase(baseCount, leaf, leafIndices);
    });
    return maxElement(baseCount);
}

static std::pair<Value*, utl::small_vector<UniquePtr<InsertValue>>>
    newLeavesInserts(
        AccessTreeNode* node,
        ir::Context& irCtx,
        utl::hashmap<std::pair<Value*, Value*>, InsertValue*> const& ivMap) {
    utl::small_vector<UniquePtr<InsertValue>> result;
    auto* maxValue = mostUsedLeavesBase(node);
    auto* baseValue = maxValue ? maxValue : irCtx.undef(node->type());
    node->leafWalk(
        [&](AccessTreeNode* leaf, std::span<size_t const> leafIndices) {
        auto* ev = dyncast<ExtractValue*>(leaf->payload());
        if (ev && ranges::equal(ev->memberIndices(), leafIndices) &&
            ev->baseValue() == maxValue)
        {
            return;
        }
        auto* ins = leaf->payload();
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
        AccessTreeNode* node,
        ir::Context& irCtx,
        utl::hashmap<std::pair<Value*, Value*>, InsertValue*> const& ivMap) {
    utl::small_vector<UniquePtr<InsertValue>> result;
    auto* maxValue = mostUsedChildrenBase(node);
    auto* baseValue = maxValue ? maxValue : irCtx.undef(node->type());

    for (size_t index = 0; auto child: node->children()) {
        std::array const indices{ index++ };
        auto* ev = dyncast<ExtractValue*>(child->payload());
        if (ev && ranges::equal(ev->memberIndices(), indices) &&
            ev->baseValue() == maxValue)
        {
            continue;
        }
        auto* ins = child->payload();
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
    inserts.insert(inserts.end(),
                   std::move_iterator(chosenInserts.begin()),
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
    root->postOrderWalk(
        [&](AccessTreeNode* node, std::span<size_t const> indices) {
        if (node->children().empty()) {
            /// Create `extract_value` instructions for all nodes that have no
            /// associated value
            if (!node->payload()) {
                auto* ev = new ExtractValue(root->payload(), indices, "ev");
                insertInst->parent()->insert(insertInst, ev);
                node->setPayload(ev);
                evList.insert(ev);
            }
            return;
        }
        auto [leavesBase, leavesInserts] = newLeavesInserts(node, irCtx, ivMap);
        auto [childrenBase, childrenInserts] =
            newChildrenInserts(node, irCtx, ivMap);

        if (childrenInserts.size() < leavesInserts.size()) {
            mergeInserts(inserts, childrenInserts, leavesInserts);
            node->setPayload(childrenBase);
        }
        else {
            mergeInserts(inserts, leavesInserts, childrenInserts);
            node->setPayload(leavesBase);
        }
    });
    for (auto& insert: inserts) {
        push(insert.get());
        insertInst->parent()->insert(insertInst, insert.release());
    }
    auto* newValue = root->payload();
    if (newValue == insertInst) {
        return nullptr;
    }
    pushIfInst(insertInst->baseValue());
    pushIfInst(insertInst->insertedValue());
    if (auto* newInsert = dyncast<InsertValue*>(newValue)) {
        pushIfInst(newInsert->baseValue());
        pushIfInst(newInsert->insertedValue());
    }
    return newValue;
}

AccessTreeNode* InstCombineCtx::getAccessTree(ExtractValue* inst) {
    return getAccessTreeCommon(inst);
}

AccessTreeNode* InstCombineCtx::getAccessTree(InsertValue* inst) {
    auto* root = getAccessTreeCommon(inst);
    auto* node = root;
    for (size_t index: inst->memberIndices()) {
        node->fanOut();
        node = node->childAt(index);
    }
    node->setPayload(inst->insertedValue());
    return root;
}

template <typename Inst>
AccessTreeNode* InstCombineCtx::getAccessTreeCommon(Inst* inst) {
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
            auto tree = std::make_unique<AccessTreeNode>(base.type());
            tree->setPayload(&base);
            return tree;
        }
    }); // clang-format on
    auto* root = treeOwner.get();
    accessTrees.insert({ inst, std::move(treeOwner) });
    return root;
}
