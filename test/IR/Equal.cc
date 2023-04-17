#include "test/IR/Equal.h"

#include <ostream>

#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>

#include "IR/CFG.h"
#include "IR/Module.h"
#include "IR/Print.h"
#include "IR/Type.h"

using namespace scatha;
using namespace test;

std::ostream& test::operator<<(std::ostream& str, EqResult const& eqResult) {
    if (eqResult) {
        return str << "Success";
    }
    str << eqResult.msg;
    if (eqResult.a && eqResult.b) {
        str << " with values " << ir::toString(*eqResult.a) << " and "
            << ir::toString(*eqResult.b);
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
    if (a->category() != b->category()) {
        return false;
    }
    return true; // For now
}

EqResult FuncEqContext::valueEqual(ir::Value const* x,
                                   ir::Value const* y) const {
    // clang-format off
    return visit(*x, *y, utl::overload{
        [&](ir::Constant const& aConst, ir::Constant const& bConst) -> EqResult {
            if (typeEqual(aConst.type(), bConst.type())) {
                return EqResult::Success;
            }
            return { x, y, "Type mismatch" };
        },
        [&](ir::Value const& a, ir::Value const& b) -> EqResult {
            auto itr = valueMap.find(&a);
            if (itr == valueMap.end()) {
                return { x, y, "No matching value in RHS" };
            }
            if (itr->second != &b) {
                return { x, y, "Value mismatch" };
            }
            return EqResult::Success;
        }
    }); // clang-format on
}
