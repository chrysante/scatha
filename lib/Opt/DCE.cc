#include "Opt/DCE.h"

#include <utl/hashtable.hpp>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Validate.h"
#include "Opt/Common.h"

using namespace scatha;
using namespace opt;
using namespace ir;

namespace {

struct DCEContext {
    explicit DCEContext(Context& irCtx, Function& function): irCtx(irCtx), function(function) {}

    bool run();

    void visitBasicBlock(BasicBlock* basicBlock);

    void erase(BasicBlock*);

    Context& irCtx;
    Function& function;

    bool modified = false;
    utl::hashset<BasicBlock const*> visited;
};

} // namespace

bool opt::dce(ir::Context& context, ir::Function& function) {
    DCEContext ctx(context, function);
    bool const result = ctx.run();
    ir::assertInvariants(context, function);
    return result;
}

bool DCEContext::run() {
    visitBasicBlock(&function.entry());
    return modified;
}

void DCEContext::visitBasicBlock(BasicBlock* basicBlock) {
    if (visited.contains(basicBlock)) {
        return;
    }
    visited.insert(basicBlock);
    if (auto* pred = basicBlock->singlePredecessor(); pred && pred->hasSingleSuccessor()) {
        SC_ASSERT(basicBlock->phiNodes().empty(), "How can we have phi nodes if we only have one predecessor?");
        pred->erase(pred->terminator());
        basicBlock->splice(basicBlock->begin(), pred);
        basicBlock->setPredecessors(pred->predecessors());
        if (pred->isEntry()) {
            basicBlock->setName(std::string(pred->name()));
        }
        for (auto* predPred: pred->predecessors()) {
            predPred->terminator()->updateTarget(pred, basicBlock);
        }
        function.basicBlocks().erase(pred);
        modified = true;
    }
    auto* const terminator = basicBlock->terminator();
    // clang-format off
    visit(*terminator, utl::overload{
        [&](Goto& gt) {
            visitBasicBlock(gt.target());
        },
        [&](Branch& br) {
            auto* constant = dyncast<IntegralConstant const*>(br.condition());
            if (!constant) {
                for (auto* succ: br.targets()) {
                    visitBasicBlock(succ);
                }
                return;
            }
            size_t const index = constant->value().to<size_t>();
            BasicBlock* target      = br.targets()[1 - index];
            BasicBlock* staleTarget = br.targets()[index];
            auto itr = basicBlock->erase(&br);
            basicBlock->insert(itr, new Goto(irCtx, target));
            modified = true;
            if (staleTarget->hasSinglePredecessor()) {
                erase(staleTarget);
            }
            else {
                removePredecessorAndUpdatePhiNodes(staleTarget, basicBlock);
            }
            visitBasicBlock(target);
        },
        [&](Return&) {},
        [&](TerminatorInst&) { SC_UNREACHABLE(); },
    }); // clang-format on
}

void DCEContext::erase(BasicBlock* basicBlock) {
    auto* const terminator = basicBlock->terminator();
    basicBlock->clearAllOperands();
    for (auto* target: terminator->targets()) {
        if (target->hasSinglePredecessor()) {
            erase(target);
        }
        else {
            removePredecessorAndUpdatePhiNodes(target, basicBlock);
        }
    }
    function.basicBlocks().erase(basicBlock);
}
