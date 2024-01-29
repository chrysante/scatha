#include "IR/Equal.h"

#include <ostream>

#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>

#include "IR/CFG.h"
#include "IR/Module.h"
#include "IR/Print.h"
#include "IR/Type.h"

using namespace scatha;
using namespace test;

/// \Returns the parent basic block if \p value is an instruction, otherwise
/// return null
static ir::BasicBlock const* getValueBB(ir::Value const* value) {
    if (auto* inst = dyncast<ir::Instruction const*>(value)) {
        return inst->parent();
    }
    return nullptr;
}

static constexpr utl::streammanip inBBMessage = [](std::ostream& str,
                                                   ir::BasicBlock const* BB) {
    if (BB) {
        str << "in basic block \"" << BB->name() << "\"";
    }
};

std::ostream& test::operator<<(std::ostream& str, EqResult const& eqResult) {
    if (eqResult) {
        return str << "Success";
    }
    str << eqResult.msg << "\n";
    if (eqResult.a) {
        str << "    Value: ";
        printDecl(*eqResult.a, str);
        str << " " << inBBMessage(getValueBB(eqResult.a)) << "\n";
    }
    if (eqResult.b) {
        str << "    Value: ";
        printDecl(*eqResult.b, str);
        str << " " << inBBMessage(getValueBB(eqResult.b)) << "\n";
    }
    return str;
}

EqResult test::modEqual(ir::Module const& A, ir::Module const& B) {
    for (auto&& [F, G]: ranges::views::zip(A, B)) {
        if (auto res = funcEqual(F, G); !res) {
            return res;
        }
    }
    return EqResult::Success;
}

namespace {

struct FuncEqContext {
    FuncEqContext(ir::Function const& F, ir::Function const& G): F(F), G(G) {}

    void index();

    EqResult compareParameters();

    EqResult compareBasicBlocks();

    EqResult compareInstructions();

    bool typeEqual(ir::Type const* a, ir::Type const* b) const;

    EqResult valueEqual(ir::Value const* x, ir::Value const* y) const;

    ir::Function const& F;
    ir::Function const& G;

    utl::hashmap<ir::Value const*, ir::Value const*> valueMap;
};

} // namespace

EqResult test::funcEqual(ir::Function const& F, ir::Function const& G) {
    FuncEqContext ctx(F, G);
    ctx.index();
    if (auto res = ctx.compareParameters(); !res) {
        return res;
    }
    if (auto res = ctx.compareBasicBlocks(); !res) {
        return res;
    }
    if (auto res = ctx.compareInstructions(); !res) {
        return res;
    }
    return EqResult::Success;
}

void FuncEqContext::index() {
    for (auto&& [P, Q]: ranges::views::zip(F.parameters(), G.parameters())) {
        valueMap[&P] = &Q;
    }
    for (auto&& [B, C]: ranges::views::zip(F, G)) {
        valueMap[&B] = &C;
    }
    for (auto&& [I, J]: ranges::views::zip(F.instructions(), G.instructions()))
    {
        valueMap[&I] = &J;
    }
}

EqResult FuncEqContext::compareParameters() {
    auto fItr = F.parameters().begin();
    auto fEnd = F.parameters().end();
    auto gItr = G.parameters().begin();
    auto gEnd = G.parameters().end();
    for (; fItr != fEnd && gItr != gEnd; ++fItr, ++gItr) {
        if (!typeEqual(fItr->type(), gItr->type())) {
            return { &*fItr, &*gItr, "Parameter type mismatch" };
        }
    }
    if (fItr != fEnd || gItr != gEnd) {
        return { nullptr, nullptr, "Parameter count mismatch" };
    }
    return EqResult::Success;
}

EqResult FuncEqContext::compareBasicBlocks() {
    if (ranges::distance(F) == ranges::distance(G)) {
        return EqResult::Success;
    }
    return { nullptr, nullptr, "Basic block count mismatch" };
}

EqResult FuncEqContext::compareInstructions() {
    for (auto&& [I, J]: ranges::views::zip(F.instructions(), G.instructions()))
    {
        if (!valueEqual(I.parent(), J.parent())) {
            return { &I, &J, "Basic block mismatch" };
        }
        if (!typeEqual(I.type(), J.type())) {
            return { &I, &J, "Instruction type mismatch" };
        }
        if (I.operands().size() != J.operands().size()) {
            return { &I, &J, "Operand count mismatch" };
        }
        for (auto&& [iOp, jOp]: ranges::views::zip(I.operands(), J.operands()))
        {
            if (!valueEqual(iOp, jOp)) {
                return { &I, &J, "Operand mismatch" };
            }
        }
    }
    return EqResult::Success;
}

bool FuncEqContext::typeEqual(ir::Type const* a, ir::Type const* b) const {
    if (a == nullptr || b == nullptr) {
        return a == b;
    }
    if (a->category() != b->category()) {
        return false;
    }
    return true; // For now
}

EqResult FuncEqContext::valueEqual(ir::Value const* x,
                                   ir::Value const* y) const {
    if (isa<ir::Global>(x) && isa<ir::Global>(y)) {
        if (typeEqual(x->type(), y->type())) {
            return EqResult::Success;
        }
        return { x, y, "Type mismatch" };
    }
    if (isa<ir::Constant>(x) && isa<ir::Constant>(y)) {
        if (typeEqual(x->type(), y->type())) {
            return EqResult::Success;
        }
        return { x, y, "Type mismatch" };
    }
    auto itr = valueMap.find(x);
    if (itr == valueMap.end()) {
        return { x, y, "No matching value in RHS" };
    }
    if (itr->second != y) {
        return { x, y, "Value mismatch" };
    }
    return EqResult::Success;
}
