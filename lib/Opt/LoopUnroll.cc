#include <span>

#include <range/v3/algorithm.hpp>
#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include "IR/CFG.h"
#include "IR/Clone.h"
#include "IR/Context.h"
#include "IR/Loop.h"
#include "IR/PassRegistry.h"
#include "Opt/Passes.h"

using namespace scatha;
using namespace ir;
using namespace opt;

SC_REGISTER_PASS(opt::loopUnroll, "loopunroll", PassCategory::Simplification);

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
    unroll(inductionValues);
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

void UnrollContext::unroll(std::span<APInt const> inductionValues) const {
    auto innerBBs = loop.innerBlocks() | ToSmallVector<>;
    auto insertPoint = innerBBs.back()->next();
    auto enteringBlocks = loop.enteringBlocks();
    BasicBlock* lastHeader = nullptr;
    for (auto [index, indValue]: inductionValues | ranges::views::enumerate) {
        CloneValueMap cloneMap;
        utl::hashmap<BasicBlock*, BasicBlock*> BBMap;
        auto tryMap = [&]<typename T>(T* value) -> T* {
            if constexpr (std::derived_from<BasicBlock, T>) {
                if (auto* BB = dyncast<BasicBlock*>(value)) {
                    auto itr = BBMap.find(BB);
                    if (itr != BBMap.end()) {
                        return itr->second;
                    }
                }
            }
            return cast<T*>(cloneMap(value));
        };
        auto map = [&]<typename T>(T* value) -> T* {
            if constexpr (std::derived_from<BasicBlock, T>) {
                if (auto* BB = dyncast<BasicBlock*>(value)) {
                    auto itr = BBMap.find(BB);
                    if (itr != BBMap.end()) {
                        return itr->second;
                    }
                }
            }
            auto result = cloneMap(value);
            SC_ASSERT(result != value, "Not found");
            return cast<T*>(result);
        };
        utl::small_vector<BasicBlock*> clonedBBs;
        utl::hashset<BasicBlock*> cloneSet;
        /// Clone all inner blocks
        for (auto* BB: innerBBs) {
            auto* clonedBB = clone(ctx, BB, cloneMap).release();
            clonedBBs.push_back(clonedBB);
            cloneSet.insert(clonedBB);
            BBMap.insert({ BB, clonedBB });
            function.insert(insertPoint, clonedBB);
        }
        auto* header = map(loop.header());
        ///
        for (auto* entering: enteringBlocks) {
            auto* term = entering->terminator();
            if (index == 0) {
                term->mapTargets(tryMap);
                continue;
            }
            /// Else
            for (auto [index, target]:
                 term->targets() | ranges::views::enumerate)
            {
                if (target == lastHeader) {
                    term->setTarget(index, header);
                }
            }
        }
        /// Update all operands according to clone map
        for (auto* clone: clonedBBs) {
            for (auto& inst: *clone) {
                for (auto [index, op]:
                     inst.operands() | ranges::views::enumerate)
                {
                    inst.setOperand(index, tryMap(op));
                }
                if (auto* phi = dyncast<Phi*>(&inst)) {
                    phi->mapPredecessors(tryMap);
                }
            }
            clone->mapPredecessors(tryMap);
        }
        ///
        for (auto* pred: header->predecessors() | ToSmallVector<>) {
            if (cloneSet.contains(pred)) {
                header->removePredecessor(pred);
            }
        }
        /// Replace the induction variable with the current value
        auto* ind = map(inductionVar);
        ind->replaceAllUsesWith(ctx.intConstant(indValue));
        /// Replace the exit condition with a constant
        auto* cond = map(exitCondition);
        cond->replaceAllUsesWith(
            ctx.boolConstant(index == inductionValues.size() - 1));
        /// The latches of the current iteration become the entering blocks of
        /// the next iteration
        enteringBlocks.clear();
        for (auto* latch: loop.latches()) {
            auto* clone = map(latch);
            enteringBlocks.insert(clone);
        }
        lastHeader = header;
    }
}
