#include "Opt/Passes.h"

#include <range/v3/algorithm.hpp>

#include "Common/Ranges.h"
#include "IR/CFG.h"
#include "IR/PassRegistry.h"
#include "IR/Validate.h"
#include "Opt/AllocaPromotion.h"

using namespace scatha;
using namespace ir;
using namespace opt;

SC_REGISTER_PASS(opt::memToReg, "memtoreg", PassCategory::Simplification);

bool opt::memToReg(Context& ctx, Function& function) {
    auto& domInfo = function.getOrComputeDomInfo();
    auto allocas = function.entry() | Filter<Alloca> | TakeAddress |
                   ToSmallVector<>;
    bool modified = false;
    while (true) {
        auto promoEnd = ranges::partition(allocas, isPromotable);
        if (promoEnd == allocas.begin()) {
            break;
        }
        for (auto* inst: ranges::make_subrange(allocas.begin(), promoEnd)) {
            promoteAlloca(inst, ctx, domInfo);
        }
        allocas.erase(allocas.begin(), promoEnd);
        modified = true;
    }
    if (modified) {
        assertInvariants(ctx, function);
    }
    return modified;
}
