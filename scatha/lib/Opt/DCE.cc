#include "Opt/Passes.h"

#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>

#include "Common/Base.h"
#include "Common/Ranges.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Dominance.h"
#include "IR/PassRegistry.h"
#include "IR/Validate.h"
#include "Opt/Common.h"
#include "Opt/Passes.h"

using namespace scatha;
using namespace opt;
using namespace ir;

/// Implemented with help from this paper
/// https://yunmingzhang.files.wordpress.com/2013/12/dcereport-2.pdf

SC_REGISTER_PASS(opt::dce, "dce", PassCategory::Simplification);

namespace {

struct DCEContext {
    explicit DCEContext(Context& irCtx, Function& function):
        irCtx(irCtx),
        function(function),
        postDomInfo(function.getOrComputePostDomInfo()) {}

    bool run();

    void mark(Instruction* inst) {
        if (marked.insert(inst).second) {
            worklist.insert(inst);
            usefulBlocks.insert(inst->parent());
        }
    }

    BasicBlock* nearestUsefulPostdom(BasicBlock* BB);

    Context& irCtx;
    Function& function;
    utl::hashset<Instruction*> worklist;
    utl::hashset<Instruction*> marked;
    utl::hashset<BasicBlock*> usefulBlocks;
    DominanceInfo const& postDomInfo;
};

} // namespace

bool opt::dce(ir::Context& context, ir::Function& function) {
    DCEContext ctx(context, function);
    bool const result = ctx.run();
    ir::assertInvariants(context, function);
    return result;
}

/// _Critical_ here means side effects as defined by `opt::hasSideEffects()` or
/// the instruction is a return
static bool isCritical(Instruction const* inst) {
    return isa<Return>(inst) || opt::hasSideEffects(inst);
}

bool DCEContext::run() {
    /// Initialization phase
    auto instructions = function.instructions() | TakeAddress |
                        ToSmallVector<32>;
    auto criticalInstructions = instructions |
                                ranges::views::filter(isCritical);
    for (auto* inst: criticalInstructions) {
        mark(inst);
    }
    if (postDomInfo.domTree().empty()) /// The function has no exits.
    {
        if (marked.empty()) {
            /// A non terminating function without critical instructions is
            /// undefined behaviour. We delete the body.
            SC_ASSERT(&function.front() != &function.back() ||
                          &function.front().front() != &function.front().back(),
                      "`function` is empty, however then it should have a "
                      "return statement");
            function.clear();
            function.pushBack(new BasicBlock(irCtx, "entry"));
            function.entry().pushBack(
                new Return(irCtx, irCtx.undef(function.returnType())));
            function.invalidateCFGInfo();
            return true;
        }
        else {
            /// We can't delete the body due to critical instruction, and we
            /// can't run the algorithm because we can't compute post dom info.
            /// We'll just leave.
            return false;
        }
    }
    /// Mark phase
    while (!worklist.empty()) {
        auto* inst = *worklist.begin();
        worklist.erase(worklist.begin());
        auto liveUnmarked = inst->operands() | Filter<Instruction> |
                            ranges::views::filter([&](auto* inst) {
            return !marked.contains(inst);
        }) | ToSmallVector<>;
        for (auto* inst: liveUnmarked) {
            mark(inst);
        }
        for (auto* BB: postDomInfo.domFront(inst->parent())) {
            mark(BB->terminator());
        }
        if (auto* phi = dyncast<Phi*>(inst)) {
            for (auto [pred, value]: phi->arguments()) {
                mark(pred->terminator());
            }
        }
    }
    /// Sweep phase
    bool modifiedAny = false;
    bool modifiedCFG = false;
    for (auto* inst: instructions) {
        if (marked.contains(inst)) {
            continue;
        }
        BasicBlock* BB = inst->parent();
        if (auto* branch = dyncast<Branch*>(inst)) {
            for (auto* target: branch->targets()) {
                target->removePredecessor(BB);
            }
            BasicBlock* target = nearestUsefulPostdom(BB);
            BB->erase(branch);
            BB->pushBack(new Goto(irCtx, target));
            target->addPredecessor(BB);
            modifiedAny = true;
            modifiedCFG = true;
        }
        else if (!isa<Goto>(inst)) {
            BB->erase(inst);
            modifiedAny = true;
        }
    }
    if (modifiedCFG) {
        function.invalidateCFGInfo();
    }
    return modifiedAny;
}

BasicBlock* DCEContext::nearestUsefulPostdom(BasicBlock* origin) {
    auto& postDomTree = postDomInfo.domTree();
    auto* node = postDomTree[origin]->parent();
    SC_ASSERT(node,
              "origin must always be post dominated, otherwise there can't be "
              "a branch from origin. We assume that the function has a single "
              "exit block.");
    do {
        auto* dest = node->basicBlock();
        if (usefulBlocks.contains(dest)) {
            return dest;
        }
        node = node->parent();
    } while (node);
    SC_UNREACHABLE();
}
