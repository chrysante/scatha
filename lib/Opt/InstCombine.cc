#include "Opt/InstCombine.h"

#include <utl/hashtable.hpp>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Validate.h"
#include "Opt/Common.h"

using namespace scatha;
using namespace ir;
using namespace opt;

namespace {

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

    Context& irCtx;
    Function& function;
    utl::hashset<Instruction*> worklist;
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
    auto* cval = dyncast<IntegralConstant const*>(value);
    if (!cval) {
        return false;
    }
    return cval->value() == static_cast<uint64_t>(constant);
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
        if (lhs == rhs && intType) {
            return irCtx.integralConstant(0, intType->bitWidth());
        }
        break;
    case ArithmeticOperation::FSub:
        if (isConstant(rhs, 0)) {
            return lhs;
        }
        if (lhs == rhs && floatType) {
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

Value* InstCombineCtx::visitImpl(ExtractValue* extractInst) {
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
#if 0
    utl::small_vector<InsertValue*> insts;
    for (; insertInst != nullptr; insertInst = dyncast<InsertValue*>(insertInst->baseValue())) {
        insts.push_back(insertInst);
    }
#endif
    return nullptr;
}
