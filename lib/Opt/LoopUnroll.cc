#include <span>

#include <range/v3/algorithm.hpp>
#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include "IR/Builder.h"
#include "IR/CFG.h"
#include "IR/Clone.h"
#include "IR/Context.h"
#include "IR/Loop.h"
#include "IR/PassRegistry.h"
#include "IR/Validate.h"
#include "Opt/Passes.h"

using namespace scatha;
using namespace ir;
using namespace opt;

SC_REGISTER_PASS(opt::loopUnroll, "loopunroll", PassCategory::Experimental);

namespace {

struct LoopCloneResult {
    CloneValueMap map;
    LoopInfo loop;
};

} // namespace

static LoopCloneResult cloneLoop(Context& ctx,
                                 BasicBlock const* insertPoint,
                                 LoopInfo const& loop) {
    auto [map, clones] =
        cloneRegion(ctx, insertPoint, loop.innerBlocks() | ToSmallVector<>);
    auto Map = ranges::views::transform(map);
    using LCPMapType =
        utl::hashmap<std::pair<BasicBlock const*, Instruction const*>, Phi*>;
    auto loopClosingPhiMap =
        loop.loopClosingPhiMap() |
        ranges::views::transform([&map = map](auto& elem) {
            auto& [key, phi] = elem;
            auto& [exit, inst] = key;
            return std::pair{ std::pair{ exit, map(inst) }, phi };
        }) |
        ranges::to<LCPMapType>;
    LoopInfo cloneLoop(map(loop.header()),
                       loop.innerBlocks() | Map |
                           ranges::to<utl::hashset<BasicBlock*>>,
                       loop.enteringBlocks(),
                       loop.latches() | Map |
                           ranges::to<utl::hashset<BasicBlock*>>,
                       loop.exitingBlocks() | Map |
                           ranges::to<utl::hashset<BasicBlock*>>,
                       loop.exitBlocks(),
                       std::move(loopClosingPhiMap),
                       loop.inductionVariables() | Map | ToSmallVector<>);
    return { std::move(map), std::move(cloneLoop) };
}

namespace {

enum class CounterDir { Increasing, Decreasing };

struct UnrollContext {
    LoopInfo const& loop;
    Context& ctx;
    Function& function;

    BasicBlock* exitingBlock = nullptr;
    CompareInst* exitCondition = nullptr;
    ArithmeticInst* inductionVar = nullptr;
    IntegralConstant* beginValue = nullptr;
    IntegralConstant* endValue = nullptr;
    IntegralConstant* strideValue = nullptr;
    CounterDir counterDir{};

    UnrollContext(LoopInfo const& loop, Context& ctx, Function& function):
        loop(loop), ctx(ctx), function(function) {}

    bool run();

    bool gatherVariables();

    APInt getTripCount() const;

    utl::small_vector<APInt> unrolledInductionValues(APInt tripCount) const;

    void unroll(std::span<APInt const> inductionValues) const;
    void unroll2(std::span<APInt const> inductionValues) const;
};

} // namespace

bool opt::loopUnroll(Context& ctx, Function& function) {
    bool modified = false;
    auto& LNF = function.getOrComputeLNF();
    LNF.postorderDFS([&](LNFNode* node) {
        if (node->isProperLoop()) {
            modified |= UnrollContext(node->loopInfo(), ctx, function).run();
        }
    });
    if (modified) {
        function.invalidateCFGInfo();
    }
    assertInvariants(ctx, function);
    return modified;
}

/// We expect the LCSSA pass to have run before this pass and that there were no
/// modifications to the CFG since then
bool UnrollContext::run() {
    if (!gatherVariables()) {
        return false;
    }
    auto tripCount = getTripCount();
    if (ucmp(tripCount, 32) > 0) {
        return false;
    }
    auto inductionValues = unrolledInductionValues(tripCount);
    unroll2(inductionValues);
    return true;
}

bool UnrollContext::gatherVariables() {
    /* exitingBlock */ {
        auto& exitingBlocks = loop.exitingBlocks();
        /// For now!
        if (exitingBlocks.size() != 1) {
            return false;
        }
        exitingBlock = *exitingBlocks.begin();
    }
    /* exitCondition */ {
        auto* branch = cast<Branch*>(exitingBlock->terminator());
        exitCondition = dyncast<CompareInst*>(branch->condition());
        if (!exitCondition) {
            return false;
        }
    }
    /* endValue */ {
        endValue = dyncast<IntegralConstant*>(exitCondition->rhs());
        if (!endValue) {
            return false;
        }
    }
    /* inductionVar */ {
        auto indVars = loop.inductionVariables();
        auto itr = ranges::find(indVars, exitCondition->lhs());
        if (itr == indVars.end()) {
            return false;
        }
        inductionVar = dyncast<ArithmeticInst*>(*itr);
        if (!inductionVar) {
            return false;
        }
    }
    /* strideValue */ {
        strideValue = dyncast<IntegralConstant*>(inductionVar->rhs());
        if (!strideValue) {
            return false;
        }
    }
    /* counterDir */ {
        using enum ArithmeticOperation;
        switch (inductionVar->operation()) {
        case Add:
            counterDir = CounterDir::Increasing;
            break;
        case Sub:
            counterDir = CounterDir::Decreasing;
            break;
        default:
            return false;
        }
    }
    /* beginValue */ {
        auto* phi = dyncast<Phi*>(inductionVar->lhs());
        if (phi->operands().size() != 2) {
            return false;
        }
        auto itr = ranges::find_if(phi->operands(), [&](auto* op) {
            return op != inductionVar;
        });
        if (itr == phi->operands().end()) {
            return false;
        }
        beginValue = dyncast<IntegralConstant*>(*itr);
        if (!beginValue) {
            return false;
        }
    }
    return true;
}

APInt UnrollContext::getTripCount() const {
    auto begin = beginValue->value();
    auto end = endValue->value();
    auto stride = strideValue->value();
    auto dist = sub(end, begin);
    SC_ASSERT(counterDir == CounterDir::Increasing, "For now...");
    if (counterDir == CounterDir::Decreasing) {
        stride = negate(stride);
    }
    using enum CompareOperation;
    switch (exitCondition->operation()) {
    case Less:
        return udiv(dist, stride);
    case LessEq:
        SC_UNIMPLEMENTED();
    case Greater:
        SC_UNIMPLEMENTED();
    case GreaterEq:
        SC_UNIMPLEMENTED();
    case Equal:
        SC_UNIMPLEMENTED();
    case NotEqual:
        return udiv(dist, stride);
    case _count:
        SC_UNREACHABLE();
    }
}

utl::small_vector<APInt> UnrollContext::unrolledInductionValues(
    APInt tripCount) const {
    auto begin = beginValue->value();
    auto end = endValue->value();
    auto stride = strideValue->value();
    auto dist = sub(end, begin);
    auto inc = [&] {
        using enum ArithmeticOperation;
        switch (inductionVar->operation()) {
        case Add:
            begin.add(stride);
            break;
        case Sub:
            begin.sub(stride);
            break;
        default:
            SC_UNREACHABLE();
        }
    };
    auto evalCond = [&] {
        int res = [&] {
            using enum CompareMode;
            switch (exitCondition->mode()) {
            case Signed:
                return scmp(begin, end);
            case Unsigned:
                return ucmp(begin, end);
            default:
                SC_UNREACHABLE();
            }
        }();
        using enum CompareOperation;
        switch (exitCondition->operation()) {
        case Less:
            return res < 0;
        case LessEq:
            return res <= 0;
        case Greater:
            return res > 0;
        case GreaterEq:
            return res >= 0;
        case Equal:
            return res == 0;
        case NotEqual:
            return res != 0;
        case _count:
            SC_UNREACHABLE();
        }
    };
    utl::small_vector<APInt> values;
    values.reserve(tripCount.to<size_t>());
    for (; evalCond(); inc()) {
        values.push_back(begin);
    }
    return values;
}

void UnrollContext::unroll2(std::span<APInt const> inductionValues) const {
    std::vector<LoopCloneResult> clones;
    clones.reserve(inductionValues.size());
    auto insertPoint = (loop.innerBlocks() | ToSmallVector<>).back()->next();
    for (size_t i = 0; i < inductionValues.size(); ++i) {
        clones.push_back(cloneLoop(ctx, insertPoint, loop));
    }
    /// Direct all entering blocks to the first loop iteration
    for (auto* entering: loop.enteringBlocks()) {
        auto* term = entering->terminator();
        term->updateTarget(loop.header(), clones.front().loop.header());
    }
    ///
    for (auto [iteration, next]:
         ranges::views::zip(clones, clones | ranges::views::drop(1)))
    {
        auto* currentHeader = iteration.loop.header();
        auto* nextHeader = next.loop.header();
        for (auto [cloneLatch, origLatch]:
             ranges::views::zip(iteration.loop.latches(), loop.latches()))
        {
            cloneLatch->terminator()->updateTarget(currentHeader, nextHeader);
            currentHeader->removePredecessor(cloneLatch);
            nextHeader->addPredecessor(cloneLatch);
            for (auto [phi, origPhi]:
                 ranges::views::zip(nextHeader->phiNodes(),
                                    loop.header()->phiNodes()))
            {
                phi.addArgument(cloneLatch,
                                iteration.map(origPhi.operandOf(origLatch)));
            }
        }
        for (auto* entering: loop.enteringBlocks()) {
            nextHeader->removePredecessor(entering);
        }
    }
    for (auto& clone: clones) {
        /// We add every exiting block to the predecessor list of the
        /// corresponding exit blocks
        for (auto* exiting: clone.loop.exitingBlocks()) {
            for (auto* succ: exiting->successors()) {
                if (clone.loop.isExit(succ)) {
                    succ->addPredecessor(exiting);
                }
            }
        }
        /// We add arguments to every phi loop exit phi node
        for (auto* BB: clone.loop.innerBlocks()) {
            for (auto& inst: *BB) {
                for (auto* exit: clone.loop.exitBlocks()) {
                    if (auto* phi = clone.loop.loopClosingPhiNode(exit, &inst))
                    {
                        auto* originalExiting = [&] {
                            for (size_t i = 0; i < phi->argumentCount(); ++i) {
                                if (loop.isInner(phi->argumentAt(i).pred)) {
                                    return phi->argumentAt(i).pred;
                                }
                            }
                            SC_UNREACHABLE();
                        }();
                        auto* cloneExiting = clone.map[originalExiting];
                        phi->addArgument(cloneExiting, &inst);
                    }
                }
            }
        }
    }
    /// After unrolling we erase the original loop
    for (auto* BB: loop.innerBlocks()) {
        for (auto* target: BB->terminator()->targets()) {
            if (target && !loop.isInner(target)) {
                target->removePredecessor(BB);
            }
        }
        function.erase(BB);
    }
}

void UnrollContext::unroll(std::span<APInt const> inductionValues) const {
    auto innerBBs = loop.innerBlocks() | ToSmallVector<>;
    auto insertPoint = innerBBs.back()->next();
    auto enteringBlocks = loop.enteringBlocks();
    BasicBlock* lastHeader = nullptr;
    CloneValueMap lastMap;
    for (auto [index, indValue]: inductionValues | ranges::views::enumerate) {
        auto cloneResult = cloneRegion(ctx, insertPoint, innerBBs);
        auto& map = cloneResult.map;
        auto clonedBBs = cloneResult.clones |
                         ranges::to<utl::hashset<BasicBlock*>>;

        auto* header = map[loop.header()];
        /// The entering blocks are
        /// - for the first iteration: the actual entering blocks
        /// - for the subsequent iterations: the latches of the previous
        /// interation For every entering block we remap the edges go to a block
        /// in the previous iteration to the corresponding block of this
        /// iteration
        for (auto* entering: enteringBlocks) {
            auto* term = entering->terminator();
            if (index == 0) {
                term->mapTargets(map);
                continue;
            }
            // index != 0
            for (auto [index, target]:
                 term->targets() | ranges::views::enumerate)
            {
                if (target == lastHeader) {
                    term->setTarget(index, header);
                }
            }
        }
        /// We remove all ...
        for (auto* pred: header->predecessors() | ToSmallVector<>) {
            if (index == 0) {
                if (clonedBBs.contains(pred)) {
                    header->removePredecessor(pred);
                }
            }
            else {
                if (clonedBBs.contains(pred)) {
                    header->updatePredecessor(pred, lastMap(map.inverse(pred)));
                }
                else {
                    header->removePredecessor(pred);
                }
            }
        }
        /// Replace the induction variable with the current value
        auto* ind = map[inductionVar];
        ind->replaceAllUsesWith(ctx.intConstant(indValue));
        /// Replace the exit condition with a constant
        auto* cond = map[exitCondition];
        for (auto* branch: cond->users() | Filter<Branch> | ToSmallVector<>) {
            auto* dest = [&, index = index] {
                if (index != inductionValues.size() - 1) {
                    return branch->thenTarget();
                }
                else {
                    return branch->elseTarget();
                }
            }();
            BasicBlockBuilder builder(ctx, branch->parent());
            builder.insert<Goto>(branch, dest);
            branch->parent()->erase(branch);
        }
        /// The latches of the current iteration become the entering blocks of
        /// the next iteration
        enteringBlocks.clear();
        for (auto* latch: loop.latches()) {
            enteringBlocks.insert(map[latch]);
        }
        lastHeader = header;
        lastMap = map;
    }
    /// After unrolling we erase the original loop
    for (auto* BB: innerBBs) {
        for (auto* target: BB->terminator()->targets()) {
            if (target && !loop.isInner(target)) {
                target->removePredecessor(BB);
            }
        }
        function.erase(BB);
    }
}
