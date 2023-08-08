#include "Opt/GlobalValueNumbering.h"

#include <array>
#include <bit>
#include <queue>

#include <iostream>

#include <range/v3/algorithm.hpp>
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
        return utl::visit(
            *inst,
            utl::overload{
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
            return ranges::equal(inst.memberIndices(),
                                 cast<ExtractValue const*>(RHS.inst)
                                     ->memberIndices());
        },
            [&](InsertValue const& inst) {
            return ranges::equal(inst.memberIndices(),
                                 cast<InsertValue const*>(RHS.inst)
                                     ->memberIndices());
            }, [&](Instruction const& inst) -> bool {
                SC_UNREACHABLE();
            } }); // clang-format on
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

    utl::hashset<std::pair<BasicBlock*, BasicBlock*>> backEdges;

    utl::hashmap<Value*, size_t> ranks;

    utl::hashmap<BasicBlock*, LocalComputationTable> LCTs;

    GVNContext(Context& context, Function& function):
        ctx(context), function(function) {}

    bool run();

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
    result |= removeCriticalEdges(ctx, function);
    GVNContext gvnContext(ctx, function);
    result |= gvnContext.run();
    assertInvariants(ctx, function);
    return result;
}

bool GVNContext::run() {
    // assignRanks();
    // buildLCTs();
    // elimLocal();
    return modified;
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
