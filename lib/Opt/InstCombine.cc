#include "Opt/InstCombine.h"

#include <iostream>

#include <range/v3/algorithm.hpp>
#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Print.h"
#include "IR/Type.h"
#include "IR/Validate.h"
#include "Opt/AccessTree.h"
#include "Opt/Common.h"

using namespace scatha;
using namespace ir;
using namespace opt;

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

    AccessTreeNode* getAccessTree(ExtractValue* inst);
    AccessTreeNode* getAccessTree(InsertValue* inst);

    template <typename Inst>
    AccessTreeNode* getAccessTreeCommon(Inst* inst);

    Context& irCtx;
    Function& function;
    utl::hashset<Instruction*> worklist;

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
    worklist = function.instructions() |
               ranges::views::transform([](auto& inst) { return &inst; }) |
               ranges::to<utl::hashset<Instruction*>>;
    bool modifiedAny = false;
    while (!worklist.empty()) {
        Instruction* inst = *worklist.begin();
        worklist.erase(worklist.begin());
        auto* replacement = visitInstruction(inst);
        if (!replacement) {
            continue;
        }
        replaceValue(inst, replacement);
        worklist.insert(inst->users().begin(), inst->users().end());
        inst->parent()->erase(inst);
        modifiedAny = true;
    }
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

Value* InstCombineCtx::visitImpl(ArithmeticInst* inst) {
    auto* const lhs       = inst->lhs();
    auto* const rhs       = inst->rhs();
    auto* const intType   = dyncast<IntegralType const*>(inst->type());
    auto* const floatType = dyncast<FloatType const*>(inst->type());
    switch (inst->operation()) {
    case ArithmeticOperation::Add:
        if (isConstant(lhs, 0)) {
            return rhs;
        }
        if (isConstant(rhs, 0)) {
            return lhs;
        }
        break;

    case ArithmeticOperation::Sub:
        if (isConstant(rhs, 0)) {
            return lhs;
        }
        if (lhs == rhs) {
            SC_ASSERT(intType, "");
            return irCtx.integralConstant(0, intType->bitWidth());
        }
        break;
    case ArithmeticOperation::FSub:
        if (isConstant(rhs, 0)) {
            return lhs;
        }
        if (lhs == rhs) {
            SC_ASSERT(floatType, "");
            return irCtx.floatConstant(0.0, floatType->bitWidth());
        }
        break;

    case ArithmeticOperation::Mul:
        if (isConstant(lhs, 1)) {
            return rhs;
        }
        if (isConstant(rhs, 1)) {
            return lhs;
        }
        break;

    case ArithmeticOperation::FMul:
        if (isConstant(lhs, 1)) {
            return rhs;
        }
        if (isConstant(rhs, 1)) {
            return lhs;
        }
        break;

    case ArithmeticOperation::SDiv:
        [[fallthrough]];
    case ArithmeticOperation::UDiv:
        if (isConstant(rhs, 1)) {
            return lhs;
        }
        if (lhs == rhs && intType) {
            return irCtx.integralConstant(1, intType->bitWidth());
        }
        break;
    case ArithmeticOperation::FDiv:
        if (isConstant(rhs, 1)) {
            return lhs;
        }
        if (lhs == rhs) {
            return irCtx.floatConstant(1.0, floatType->bitWidth());
        }
        break;

    default:
        break;
    }
    return nullptr;
}

Value* InstCombineCtx::visitImpl(Phi* phi) {
    Value* const first = phi->operands().front();
    bool const allEqual =
        ranges::all_of(phi->operands(), [&](auto* op) { return op == first; });
    if (!allEqual) {
        return nullptr;
    }
    return first;
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
    auto* tree = getAccessTree(extractInst);

    return nullptr;

    for (auto* insertInst = dyncast<InsertValue*>(extractInst->baseValue());
         insertInst != nullptr;
         insertInst = dyncast<InsertValue*>(insertInst->baseValue()))
    {
        if (ranges::equal(extractInst->memberIndices(),
                          insertInst->memberIndices()))
        {
            return insertInst->insertedValue();
        }
    }
    return nullptr;
}

Value* InstCombineCtx::visitImpl(InsertValue* insertInst) {
    auto* root = getAccessTree(insertInst);

    print(insertInst, root);

    utl::small_vector<size_t> indices;
    auto walk = [&](AccessTreeNode* node, auto& walk) -> void {
        if (node->children().empty()) {
            if (!node->payload()) {
                auto* ev = new ExtractValue(root->payload(), indices, "ev");
                insertInst->parent()->insert(insertInst, ev);
                node->setPayload(ev);
            }
            return;
        }

        indices.push_back(0);
        for (auto* child: node->children()) {
            walk(child, walk);
            ++indices.back();
        }
        indices.pop_back();

        utl::hashmap<Value*, size_t> baseCount;
        for (auto [index, child]: node->children() | ranges::views::enumerate) {
            auto* ev = dyncast<ExtractValue*>(child->payload());
            if (!ev || ev->memberIndices().size() != 1 ||
                ev->memberIndices()[0] != index)
            {
                continue;
            }
            ++baseCount[ev->baseValue()];
        }

        auto* maxValue = [&]() -> Value* {
            if (baseCount.empty()) {
                return nullptr;
            }
            return ranges::max_element(baseCount,
                                       ranges::less{},
                                       [](auto& p) { return p.second; })
                ->first;
        }();
        auto* baseValue = maxValue ? maxValue : irCtx.undef(node->type());

        for (auto [index, child]: node->children() | ranges::views::enumerate) {
            auto* ev = dyncast<ExtractValue*>(child->payload());
            if (!ev || ev->memberIndices().size() != 1 ||
                ev->memberIndices()[0] != index || ev->baseValue() != maxValue)
            {
                auto* iv = new InsertValue(baseValue,
                                           child->payload(),
                                           { index },
                                           "iv");
                insertInst->parent()->insert(insertInst, iv);
                baseValue = iv;
            }
            else {
                child->setPayload(nullptr);
            }
        }

        node->setPayload(baseValue);
    };
    walk(root, walk);

    if (root->payload() != insertInst) {
        return root->payload();
    }
    return nullptr;
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
