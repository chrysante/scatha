#include <optional>
#include <queue>
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
#include "Opt/LoopRankView.h"
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
        loop(loop), ctx(ctx), function(function) {
        SC_EXPECT(isLCSSA(loop));
    }

    /// Run the algorithm for this loop
    bool run();

    /// Assign all pointer variables above
    bool gatherVariables();

    /// \Returns a list of the values of the induction variable for each
    /// iteration of the loop. Returns `std::nullopt` if the loop has too many
    /// iterations
    std::optional<utl::small_vector<APInt>> unrolledInductionValues() const;

    /// Performs the CFG modifications
    void unroll(std::span<APInt const> inductionValues) const;
};

} // namespace

bool opt::loopUnroll(Context& ctx, Function& function) {
    auto LRV = LoopRankView::Compute(function);
    bool modified = false;
    /// We traverse all loops in reverse rank order (reverse BFS order)
    for (auto& rankList: LRV | ranges::views::reverse) {
        auto& LNF = function.getOrComputeLNF();
        bool modifiedRank = false;
        for (auto* header: rankList) {
            modifiedRank |=
                UnrollContext(LNF[header]->loopInfo(), ctx, function).run();
        }
        /// After traversing a rank we invalidate because we may have edited
        /// the CFG in loops the are dominated by the next rank
        if (modifiedRank) {
            function.invalidateCFGInfo();
            modified = true;
        }
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
    auto inductionValues = unrolledInductionValues();
    if (!inductionValues) {
        return false;
    }
    unroll(*inductionValues);
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

std::optional<utl::small_vector<APInt>> UnrollContext::unrolledInductionValues()
    const {
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
    /// Here we perform formal loop evaluation to determine the value of the
    /// induction variable in each iteration
    utl::small_vector<APInt> values;
    size_t const MaxTripCount = 32;
    while (true) {
        /// We increment first because the induction variable is the variable
        /// that is tested in the exit condition
        inc();
        values.push_back(begin);
        if (!evalCond()) {
            break;
        }
        if (values.size() > MaxTripCount) {
            return std::nullopt;
        }
    }
    return values;
}

void UnrollContext::unroll(std::span<APInt const> inductionValues) const {
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
    /// Here we stitch together the phi nodes and terminators
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
    ///
    for (auto [clone, indValue]: ranges::views::zip(clones, inductionValues)) {
        auto* indVar = clone.map[inductionVar];
        indVar->replaceAllUsesWith(ctx.intConstant(indValue));
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
