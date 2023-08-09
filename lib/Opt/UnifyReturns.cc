#include "Opt/Passes.h"

#include <algorithm>

#include <utl/vector.hpp>

#include "IR/CFG.h"
#include "Opt/PassManager.h"

using namespace scatha;
using namespace opt;
using namespace ir;

static utl::small_vector<BasicBlock*> gatherReturnBlocks(Function& function) {
    utl::small_vector<BasicBlock*> returnBlocks;
    for (auto& bb: function) {
        if (auto* ret = dyncast<Return*>(bb.terminator())) {
            returnBlocks.push_back(&bb);
        }
    }
    return returnBlocks;
}

SC_REGISTER_PASS(opt::unifyReturns, "unifyreturns");

bool opt::unifyReturns(Context& ctx, Function& function) {
    auto returnBlocks = gatherReturnBlocks(function);
    if (returnBlocks.size() <= 1) {
        return false;
    }
    auto* returnBlock = new BasicBlock(ctx, "return");
    auto* retvalPhi = new Phi(function.returnType(), "retval");
    utl::small_vector<PhiMapping> args;
    args.reserve(returnBlocks.size());
    for (auto* oldRetBlock: returnBlocks) {
        auto* retInst = cast<Return*>(oldRetBlock->terminator());
        args.push_back({ oldRetBlock, retInst->value() });
        oldRetBlock->erase(retInst);
        oldRetBlock->pushBack(new Goto(ctx, returnBlock));
        returnBlock->addPredecessor(oldRetBlock);
    }
    retvalPhi->setArguments(args);
    returnBlock->pushBack(retvalPhi);
    returnBlock->pushBack(new Return(ctx, retvalPhi));
    function.pushBack(returnBlock);
    function.invalidateCFGInfo();
    return true;
}

SC_REGISTER_PASS(opt::splitReturns, "splitreturns");

bool opt::splitReturns(Context& ctx, Function& function) {
    bool modifiedAny = false;
    auto returnBlocks = gatherReturnBlocks(function);
    for (auto* block: returnBlocks) {
        auto* ret = cast<Return*>(block->terminator());
        auto* phi = dyncast<Phi*>(ret->value());
        if (!phi) {
            continue;
        }
        /// If there is dead code between the phi and the return this pass
        /// fails, so DCE should be run before.
        if (phi->next() != ret) {
            continue;
        }
        utl::small_vector<uint16_t> removedPreds;
        for (auto [index, pred]:
             block->predecessors() | ranges::views::enumerate)
        {
            /// We can only do this for predecessors that end in `goto`'s
            if (!isa<Goto>(pred->terminator())) {
                continue;
            }
            auto* newReturn = new Return(ctx, phi->argumentAt(index).value);
            pred->insert(pred->terminator(), newReturn);
            pred->erase(pred->terminator());
            removedPreds.push_back(utl::narrow_cast<uint16_t>(index));
        }
        /// Sort the indices so when we erase an index we don't invalide greater
        /// indices
        std::sort(removedPreds.begin(), removedPreds.end(), std::greater<>{});
        for (size_t const index: removedPreds) {
            block->removePredecessor(index);
        }
        modifiedAny |= !removedPreds.empty();
        switch (block->numPredecessors()) {
        case 0:
            function.erase(block);
            break;
        case 1: {
            auto* retval = phi->argumentAt(0).value;
            ret->setValue(retval);
            block->erase(phi);
        }
        default:
            break;
        }
    }
    if (modifiedAny) {
        function.invalidateCFGInfo();
    }
    return modifiedAny;
}
