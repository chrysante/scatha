#include "Opt/DCE.h"

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Validate.h"

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
    if (auto* pred = basicBlock->singlePredecessor();
        pred && pred->hasSingleSuccessor())
    {
        SC_ASSERT(basicBlock->phiNodes().empty(), "How can we have phi nodes if we only have one predecessor?");
        pred->erase(pred->terminator());
        basicBlock->splice(basicBlock->begin(), pred);
        basicBlock->setPredecessors(pred->predecessors());
        function.basicBlocks().erase(pred);
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
                return;
            }
            modified = true;
            size_t const index = constant->value().to<size_t>();
            BasicBlock* target      = br.targets()[1 - index];
            BasicBlock* staleTarget = br.targets()[index];
            auto itr = basicBlock->erase(&br);
            basicBlock->insert(itr, new Goto(irCtx, target));
            erase(staleTarget);
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
        if (target->predecessors().size() == 1) {
            erase(target);
        }
        else {
            target->removePredecessor(basicBlock);
            /// TODO: Here we also need to remove this basic block as from phi operands of 'target'
        }
    }
    basicBlock->parent()->basicBlocks().erase(basicBlock);
}
