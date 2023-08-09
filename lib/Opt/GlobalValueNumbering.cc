#include "Opt/GlobalValueNumbering.h"

#include <array>
#include <bit>
#include <queue>

#include <iostream>

#include <range/v3/algorithm.hpp>
#include <utl/graph.hpp>
#include <utl/hash.hpp>
#include <utl/hashtable.hpp>

#include "IR/CFG.h"
#include "IR/Clone.h"
#include "IR/Dominance.h"
#include "IR/Loop.h"
#include "IR/Validate.h"
#include "Opt/Common.h"
#include "Opt/LoopRotate.h"

using namespace scatha;
using namespace opt;
using namespace ir;

namespace {

struct Expression {
    Expression(Instruction const* inst): inst(inst) {}

    bool operator==(Expression const& RHS) const {
        if (inst->nodeType() != RHS.inst->nodeType()) {
            return false;
        }
        // clang-format off
        return utl::visit(*inst, utl::overload{
            [&](ArithmeticInst const& inst) {
                return inst.operation() ==
                       cast<ArithmeticInst const*>(RHS.inst)->operation();
            },
            [&](UnaryArithmeticInst const& inst) {
                return inst.operation() ==
                       cast<UnaryArithmeticInst const*>(RHS.inst)->operation();
            },
            [&](CompareInst const& inst) {
                return inst.operation() ==
                       cast<CompareInst const*>(RHS.inst)->operation();
            },
            [&](ExtractValue const& inst) {
                return ranges::equal(
                           inst.memberIndices(),
                           cast<ExtractValue const*>(RHS.inst)->memberIndices());
            },
            [&](InsertValue const& inst) {
                return ranges::equal(
                           inst.memberIndices(),
                           cast<InsertValue const*>(RHS.inst)->memberIndices());
            },
            [&](Instruction const& inst) -> bool {
                SC_UNREACHABLE();
            }
        }); // clang-format on
    }

private:
    Instruction const* inst;
};

} // namespace

namespace {

struct LocalComputationTable {
    static LocalComputationTable build(
        BasicBlock* BB, utl::hashmap<Value*, size_t> const& ranks);

    auto computations() const {
        return rankMap | ranges::views::transform([](auto& p) -> auto& {
                   return p.second;
               }) |
               ranges::views::join;
    }

    utl::vector<Instruction*>& computations(size_t rank) {
        return rankMap[rank];
    }

    size_t maxRank() const { return _maxRank; }

private:
    utl::hashmap<size_t, utl::small_vector<Instruction*>> rankMap;
    size_t _maxRank = 0;
};

struct GVNContext {
    Context& ctx;
    Function& function;

    bool modified = false;

    utl::hashset<BasicBlock*> edgeSplitBlocks;

    utl::small_vector<BasicBlock*> topsortOrder;

    utl::hashmap<Value*, size_t> ranks;

    utl::hashmap<BasicBlock*, LocalComputationTable> LCTs;

    GVNContext(Context& context, Function& function):
        ctx(context), function(function) {}

    bool run();

    void splitCriticalEdges();

    void joinSplitEdges();

    void computeTopsortOrder();

    void assignRanks();

    void buildLCTs();

    void elimLocal();

    size_t computeRank(Instruction* inst);

    size_t getAvailRank(Value* value);
};

} // namespace

bool opt::globalValueNumbering(Context& ctx, Function& function) {
    bool result = false;
    result |= rotateWhileLoops(ctx, function);
    GVNContext gvnContext(ctx, function);
    result |= gvnContext.run();
    assertInvariants(ctx, function);
    return result;
}

bool GVNContext::run() {
    splitCriticalEdges();
    computeTopsortOrder();
    assignRanks();
    // buildLCTs();
    // elimLocal();
    joinSplitEdges();
    return modified;
}

void GVNContext::splitCriticalEdges() {
    struct DFS {
        Context& ctx;
        utl::hashset<BasicBlock*>& insertedBlocks;
        utl::hashset<BasicBlock*> visited = {};

        void search(BasicBlock* BB) {
            if (!visited.insert(BB).second) {
                return;
            }
            for (auto* succ: BB->successors()) {
                if (isCriticalEdge(BB, succ)) {
                    insertedBlocks.insert(splitEdge(ctx, BB, succ));
                }
                search(succ);
            }
        }
    };
    DFS{ ctx, edgeSplitBlocks }.search(&function.entry());
}

static void eraseBlock(BasicBlock* BB) {
    auto* function = BB->parent();
    auto* pred = BB->singlePredecessor();
    auto* succ = BB->singleSuccessor();
    pred->terminator()->updateTarget(BB, succ);
    succ->updatePredecessor(BB, pred);
    function->erase(BB);
}

void GVNContext::joinSplitEdges() {
    auto blocks = edgeSplitBlocks | ranges::to<utl::small_vector<BasicBlock*>>;
    for (auto* BB: blocks) {
        if (BB->emptyExceptTerminator() && BB->hasSinglePredecessor() &&
            BB->hasSingleSuccessor())
        {
            edgeSplitBlocks.erase(BB);
            eraseBlock(BB);
        }
    }
    modified |= !edgeSplitBlocks.empty();
}

void GVNContext::computeTopsortOrder() {
    struct DFS {
        utl::hashset<BasicBlock*> visited;
        utl::hashset<std::pair<BasicBlock*, BasicBlock*>> backEdges;

        void search(BasicBlock* BB) {
            visited.insert(BB);
            for (auto* succ: BB->successors()) {
                if (visited.contains(succ)) {
                    backEdges.insert({ BB, succ });
                    continue;
                }
                search(succ);
            }
        }
    };

    DFS dfs;
    dfs.search(&function.entry());
    topsortOrder = function |
                   ranges::views::transform([](auto& BB) { return &BB; }) |
                   ranges::to<utl::small_vector<BasicBlock*>>;
    utl::topsort(topsortOrder.begin(), topsortOrder.end(), [&](BasicBlock* BB) {
        return BB->successors() |
               ranges::views::filter([&dfs, BB](BasicBlock* succ) {
                   return !dfs.backEdges.contains({ BB, succ });
               });
    });
}

void GVNContext::assignRanks() {
    utl::hashset<BasicBlock*> visited;
    std::queue<BasicBlock*> queue;
    visited.insert(&function.entry());
    queue.push(&function.entry());
    while (!queue.empty()) {
        auto* BB = queue.front();
        queue.pop();
        for (auto* succ: BB->successors()) {
            if (visited.insert(succ).second) {
                queue.push(succ);
            }
        }
        for (auto& inst: *BB) {
            if (isa<TerminatorInst>(inst)) {
                continue;
            }
            ranks[&inst] = computeRank(&inst);
        }
    }
}

size_t GVNContext::computeRank(Instruction* inst) {
    size_t rank = ranges::max(inst->operands() |
                              ranges::views::transform([&](Value* value) {
                                  return getAvailRank(value);
                              }));
    if (!isa<Phi>(inst)) {
        ++rank;
    }
    return rank;
}

size_t GVNContext::getAvailRank(Value* value) { return ranks[value]; }

void GVNContext::buildLCTs() {
    for (auto& BB: function) {
        LCTs.insert({ &BB, LocalComputationTable::build(&BB, ranks) });
    }
}

void GVNContext::elimLocal() {
    for (auto& BB: function) {
        auto& LCT = LCTs.find(&BB)->second;
        for (size_t rank = 0; rank < LCT.maxRank(); ++rank) {
            auto& comps = LCT.computations(rank);
            for (auto i = comps.begin(); i != comps.end(); ++i) {
                for (auto j = i + 1; j != comps.end();) {
                    bool canErase =
                        Expression(*i) == Expression(*j) &&
                        ranges::equal((*i)->operands(), (*j)->operands()) &&
                        !hasSideEffects(*i);
                    if (canErase) {
                        replaceValue(*j, *i);
                        BB.erase(*j);
                        j = comps.erase(j);
                        continue;
                    }
                    ++j;
                }
            }
        }
    }
}

LocalComputationTable LocalComputationTable::build(
    BasicBlock* BB, utl::hashmap<Value*, size_t> const& ranks) {
    LocalComputationTable result;
    for (auto [value, rank]: ranks) {
        auto* inst = dyncast<Instruction*>(value);
        if (!inst || inst->parent() != BB) {
            continue;
        }
        result.rankMap[rank].push_back(inst);
        result._maxRank = std::max(result._maxRank, rank);
    }
    return result;
}
