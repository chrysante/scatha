#include "Opt/DCE.h"

/// https://yunmingzhang.files.wordpress.com/2013/12/dcereport-2.pdf

#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>

#include "Basic/Basic.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Dominance.h"
#include "IR/Validate.h"
#include "Opt/Common.h"

using namespace scatha;
using namespace opt;
using namespace ir;

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

    BasicBlock* nearestUsefulPostdom(BasicBlock* bb);

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

static bool isCritical(Instruction const& inst) {
    // clang-format off
    return visit(inst, utl::overload{
        [](Return const&) { return true; },
        [](Store const&) { return true; },
        [](Call const& call) {
            auto* function = call.function();
            if (function->hasAttribute(FunctionAttribute::Memory_WriteNone)) {
                return false;
            }
            return true;
        },
        [](Instruction const&) { return false; },
    }); // clang-format on
}

bool DCEContext::run() {
    /// Initialization phase
    auto instructions =
        function.instructions() |
        ranges::views::transform([](auto& inst) { return &inst; }) |
        ranges::to<utl::small_vector<Instruction*, 32>>;
    auto criticalInstructions =
        instructions |
        ranges::views::filter([](auto* inst) { return isCritical(*inst); });
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
        auto liveUnmarked =
            inst->operands() | ranges::views::transform([](Value* op) {
                return dyncast<Instruction*>(op);
            }) |
            ranges::views::filter([&](auto* inst) {
                return inst != nullptr && !marked.contains(inst);
            }) |
            ranges::to<utl::small_vector<Instruction*>>;
        for (auto* inst: liveUnmarked) {
            mark(inst);
        }
        for (auto* bb: postDomInfo.domFront(inst->parent())) {
            mark(bb->terminator());
        }
        if (auto* phi = dyncast<Phi*>(inst)) {
            for (auto [pred, value]: phi->arguments()) {
                mark(pred->terminator());
            }
        }
    }
    /// Sweep phase
    bool modifiedAny = false;
    for (auto* inst: instructions) {
        if (marked.contains(inst)) {
            continue;
        }
        modifiedAny    = true;
        BasicBlock* bb = inst->parent();
        if (auto* branch = dyncast<Branch*>(inst)) {
            for (auto* target: branch->targets()) {
                target->removePredecessor(bb);
            }
            BasicBlock* target = nearestUsefulPostdom(bb);
            bb->erase(branch);
            bb->pushBack(new Goto(irCtx, target));
            target->addPredecessor(bb);
            continue;
        }
        if (!isa<Goto>(inst)) {
            clearAllUses(inst);
            bb->erase(inst);
        }
    }
    return modifiedAny;
}

BasicBlock* DCEContext::nearestUsefulPostdom(BasicBlock* origin) {
    auto& postDomTree = postDomInfo.domTree();
    auto* node        = postDomTree[origin]->parent();
    do {
        auto* dest = node->basicBlock();
        if (usefulBlocks.contains(dest)) {
            return dest;
        }
        node = node->parent();
    } while (node);
    SC_UNREACHABLE();
}
