#include "Opt/LoopCanonical.h"

#include <utl/hashtable.hpp>
#include <utl/strcat.hpp>
#include <utl/vector.hpp>

#include "IR/CFG.h"
#include "IR/Loop.h"
#include "IR/Validate.h"

using namespace scatha;
using namespace opt;
using namespace ir;

namespace {

struct LCContext {
    LCContext(Context& ctx, Function& F):
        ctx(ctx), F(F), LNF(F.getOrComputeLNF()) {}

    bool run();

    void traverseLoops(LoopNestingForest::Node const* node);

    Context& ctx;
    Function& F;
    LoopNestingForest const& LNF;
    bool modified = false;
};

} // namespace

bool opt::makeLoopCanonical(Context& ctx, Function& F) {
    bool const modified = LCContext(ctx, F).run();
    if (!modified) {
        return false;
    }
    F.invalidateCFGInfo();
    ir::assertInvariants(ctx, F);
    return true;
}

bool LCContext::run() {
    for (auto* node: LNF.roots()) {
        traverseLoops(node);
    }
    return modified;
}

void LCContext::traverseLoops(LoopNestingForest::Node const* node) {
    if (node->children().empty()) {
        return;
    }
    for (auto* child: node->children()) {
        traverseLoops(child);
    }
    auto* header    = node->basicBlock();
    auto loopBlocks = node->children() |
                      ranges::views::transform(
                          [](auto* node) { return node->basicBlock(); }) |
                      ranges::to<utl::hashset<BasicBlock*>>;
    auto const numPreds =
        ranges::count_if(header->predecessors(), [&](BasicBlock const* pred) {
            return !loopBlocks.contains(pred);
        });
    if (numPreds == 1) {
        return;
    }
    auto* preheader = new BasicBlock(ctx, "preheader");
    for (auto* pred: header->predecessors()) {
        auto* term = pred->terminator();
        term->updateOperand(header, preheader);
    }

    utl::small_vector<BasicBlock*> preheaderPreds;
    utl::small_vector<BasicBlock*> headerPreds = { preheader };
    for (auto* pred: header->predecessors()) {
        if (loopBlocks.contains(pred)) {
            headerPreds.push_back(pred);
        }
        else {
            preheaderPreds.push_back(pred);
        }
    }
    preheader->setPredecessors(preheaderPreds);
    header->setPredecessors(headerPreds);
    utl::small_vector<PhiMapping> preheaderPhiArgs(preheaderPreds.size());
    for (auto [index, pred]: preheaderPreds | ranges::views::enumerate) {
        preheaderPhiArgs[index].pred = pred;
    }
    utl::small_vector<PhiMapping> headerPhiArgs(headerPreds.size());
    for (auto [index, pred]: headerPreds | ranges::views::enumerate) {
        headerPhiArgs[index].pred = pred;
    }
    for (auto& phi: header->phiNodes()) {
        size_t preheaderIndex = 0;
        size_t headerIndex    = 1;
        for (auto [pred, value]: phi.arguments()) {
            if (!loopBlocks.contains(pred)) {
                auto& arg = preheaderPhiArgs[preheaderIndex++];
                SC_ASSERT(arg.pred == pred, "");
                arg.value = value;
            }
            else {
                auto& arg = headerPhiArgs[headerIndex++];
                SC_ASSERT(arg.pred == pred, "");
                arg.value = value;
            }
        }

        auto* prePhi =
            new Phi(preheaderPhiArgs, utl::strcat("pre.", phi.name()));
        headerPhiArgs[0].value = prePhi;
        phi.setArguments(headerPhiArgs);
        preheader->pushBack(prePhi);
    }
    preheader->pushBack(new Goto(ctx, header));
    F.insert(header, preheader);
    modified = true;
}
