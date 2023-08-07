#include "Opt/GlobalValueNumbering.h"

#include <array>
#include <bit>
#include <queue>

#include <iostream>

#include <range/v3/algorithm.hpp>
#include <utl/hash.hpp>
#include <utl/hashtable.hpp>
#include <utl/strcat.hpp>

#include "IR/CFG.h"
#include "IR/Clone.h"
#include "IR/Loop.h"
#include "Opt/Common.h"

using namespace scatha;
using namespace opt;
using namespace ir;

namespace {

struct Expression {
    Expression(Instruction const* inst) {
        id[0] = static_cast<uint32_t>(inst->nodeType());
        // clang-format off
        id[1] = utl::visit(*inst, utl::overload{
            [](ArithmeticInst const& inst) {
                return static_cast<uint32_t>(inst.operation());
            },
            [](UnaryArithmeticInst const& inst) {
                return static_cast<uint32_t>(inst.operation());
            },
            [](CompareInst const& cmp) {
                return static_cast<uint32_t>(cmp.operation());
            },
            [](Instruction const&) {
                return 0;
            }
        }); // clang-format on
    }

    size_t hashValue() const { return std::bit_cast<size_t>(id); }

    bool operator==(Expression const&) const = default;

private:
    std::array<uint32_t, 2> id;
};

} // namespace

template <>
struct std::hash<Expression> {
    size_t operator()(Expression const& expr) const { return expr.hashValue(); }
};

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

struct SingleLoopEntryResult {
    BasicBlock* preHeader = nullptr;
    utl::small_vector<BasicBlock*> outerPreds, innerPreds;
};

struct GVNContext {
    Context& ctx;
    Function& function;
    LoopNestingForest const& LNF;

    bool modified = false;

    utl::hashset<BasicBlock*> loopHeaders, landingPads;
    utl::hashset<std::pair<BasicBlock*, BasicBlock*>> backEdges;
    utl::hashmap<Value*, size_t> ranks;
    utl::hashmap<BasicBlock*, LocalComputationTable> LCTs;

    GVNContext(Context& context, Function& function):
        ctx(context), function(function), LNF(function.getOrComputeLNF()) {}

    bool run();

    void restructureCFG();
    void restructureDFS(utl::hashset<BasicBlock*>& visited, BasicBlock* BB);

    SingleLoopEntryResult makeLoopSingleEntry(BasicBlock* header);
    void addLandingPadToLoop(BasicBlock* header);

    void assignRanks();

    void buildLCTs();

    void elimLocal();

    size_t computeRank(Instruction* inst);

    size_t getAvailRank(Value* value);
};

} // namespace

bool opt::globalValueNumbering(Context& context, Function& function) {
    GVNContext gvnContext(context, function);
    return gvnContext.run();
}

bool GVNContext::run() {
    restructureCFG();
    removeCriticalEdges(ctx, function);
    assignRanks();
    buildLCTs();
    elimLocal();

    return modified;
}

void GVNContext::restructureCFG() {
    utl::hashset<BasicBlock*> visited;
    restructureDFS(visited, &function.entry());
    for (auto* root: LNF.roots()) {
        root->traversePreorder([&](LoopNestingForest::Node const* node) {
            if (node->isProperLoop()) {
                addLandingPadToLoop(node->basicBlock());
            }
        });
    }
}

void GVNContext::restructureDFS(utl::hashset<BasicBlock*>& visited,
                                BasicBlock* BB) {
    visited.insert(BB);
    for (auto* succ: BB->successors()) {
        if (visited.contains(succ)) {
            loopHeaders.insert(succ);
            backEdges.insert({ BB, succ });
            continue;
        }
        restructureDFS(visited, succ);
    }
}

SingleLoopEntryResult GVNContext::makeLoopSingleEntry(BasicBlock* header) {
    auto headerParent = LNF[header]->parent();
    SingleLoopEntryResult result;
    auto& [preHeader, outerPreds, innerPreds] = result;
    for (auto* pred: header->predecessors()) {
        if (LNF[pred]->parent() != headerParent || pred == header) {
            innerPreds.push_back(pred);
        }
        else {
            outerPreds.push_back(pred);
        }
    }
    if (outerPreds.size() <= 1) {
        preHeader = outerPreds.front();
        return result;
    }
    preHeader = new BasicBlock(ctx, "preheader");
    function.insert(header, preHeader);
    preHeader->pushBack(new Goto(ctx, header));
    auto phPhiInsertPoint = std::prev(preHeader->end());
    for (auto& phi: header->phiNodes()) {
        utl::small_vector<PhiMapping> newPhiArgs;
        for (auto* pred: outerPreds) {
            newPhiArgs.push_back({ pred, phi.operandOf(pred) });
        }
        auto* phPhi = new Phi(newPhiArgs, utl::strcat("ph.", phi.name()));
        preHeader->insert(phPhiInsertPoint, phPhi);
        utl::small_vector<PhiMapping> oldPhiArgs;
        for (auto* pred: innerPreds) {
            oldPhiArgs.push_back({ pred, phi.operandOf(pred) });
        }
        oldPhiArgs.push_back({ preHeader, phPhi });
        phi.setArguments(oldPhiArgs);
    }
    for (auto* pred: outerPreds) {
        pred->terminator()->updateTarget(header, preHeader);
    }
    innerPreds.push_back(preHeader);
    header->setPredecessors(innerPreds);
    preHeader->setPredecessors(outerPreds);
    return result;
}

void GVNContext::addLandingPadToLoop(BasicBlock* header) {
    auto [preHeader, outerPreds, innerPreds] = makeLoopSingleEntry(header);
    auto* headerClone = ir::clone(ctx, header).release();
    headerClone->setName(utl::strcat(header->name(), ".clone"));
    headerClone->removePredecessor(preHeader);
    auto* landingPad = new BasicBlock(ctx, "landingpad");
    auto* body = [&] {
        auto* succA = header->successors()[0];
        if (LNF[header]->parent() != LNF[succA]->parent()) {
            return succA;
        }
        return header->successors()[1];
    }();
    SC_ASSERT(body->predecessors().size() == 1, "");
    header->terminator()->updateTarget(body, landingPad);
    landingPad->addPredecessor(header);
    auto bodyInsertPoint = body->phiEnd();
    for (auto&& [phi, clonePhi]:
         ranges::views::zip(header->phiNodes(), headerClone->phiNodes()))
    {
        auto* bodyPhi = new Phi({ { landingPad, phi.operandOf(preHeader) },
                                  { headerClone, &clonePhi } },
                                utl::strcat("body.", phi.name()));
        body->insert(bodyInsertPoint, bodyPhi);
    }
    landingPad->pushBack(new Goto(ctx, body));
    body->setPredecessors(std::array{ landingPad, headerClone });
    for (auto* pred: innerPreds) {
        header->removePredecessor(pred);
        pred->terminator()->updateTarget(header, headerClone);
    }
    function.insert(header->next(), landingPad);
    function.insert(innerPreds.back()->next(), headerClone);
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
