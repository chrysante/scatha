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
#include "IR/Dominance.h"
#include "IR/Loop.h"
#include "IR/Validate.h"
#include "Opt/Common.h"

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

struct SingleLoopEntryResult {
    BasicBlock* preHeader = nullptr;
    utl::small_vector<BasicBlock*> outerPreds, innerPreds;
};

struct GVNContext {
    Context& ctx;
    Function& function;
    LoopNestingForest& LNF;
    DominanceInfo const& postDomInfo;

    bool modified = false;
    bool modifiedCFG = false;

    utl::hashset<BasicBlock*> loopHeaders, landingPads;
    utl::hashset<std::pair<BasicBlock*, BasicBlock*>> backEdges;
    utl::hashmap<Value*, size_t> ranks;
    utl::hashmap<BasicBlock*, LocalComputationTable> LCTs;

    GVNContext(Context& context, Function& function):
        ctx(context),
        function(function),
        LNF(const_cast<LoopNestingForest&>(function.getOrComputeLNF())),
        postDomInfo(function.getOrComputePostDomInfo()) {}

    bool run();

    void restructureCFG();
    void restructureDFS(utl::hashset<BasicBlock*>& visited, BasicBlock* BB);

    SingleLoopEntryResult makeLoopSingleEntry(BasicBlock* header);
    void addLandingPadToLoop(BasicBlock* header);
    void addLandingPadToWhileLoop(BasicBlock* header);
    /// Add a node as the loop entry node with only one predecessor. This is
    /// called when the loop entry node has multiple predecessors.
    BasicBlock* addDistinctLoopEntry(BasicBlock* header, BasicBlock* entry);
    void addLandingPadToDoWhileLoop(BasicBlock* header);

    void assignRanks();

    void buildLCTs();

    void elimLocal();

    size_t computeRank(Instruction* inst);

    size_t getAvailRank(Value* value);
};

} // namespace

bool opt::globalValueNumbering(Context& context, Function& function) {
    GVNContext gvnContext(context, function);
    bool result = gvnContext.run();
    assertInvariants(context, function);
    return result;
}

bool GVNContext::run() {
    restructureCFG();
    // modifiedCFG |= removeCriticalEdges(ctx, function);
    // assignRanks();
    // buildLCTs();
    // elimLocal();
    if (modifiedCFG) {
        function.invalidateCFGInfo();
    }
    return modified || modifiedCFG;
}

void GVNContext::restructureCFG() {
    utl::hashset<BasicBlock*> visited;
    restructureDFS(visited, &function.entry());
    for (auto* root: LNF.roots()) {
        root->traversePreorder([&](LNFNode const* node) {
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

bool isLoopNode(LNFNode const* node, LNFNode const* header) {
    while (node != nullptr) {
        if (node == header) {
            return true;
        }
        node = node->parent();
    }
    return false;
}

SingleLoopEntryResult GVNContext::makeLoopSingleEntry(BasicBlock* header) {
    auto headerNode = LNF[header];
    SingleLoopEntryResult result;
    auto& [preHeader, outerPreds, innerPreds] = result;
    for (auto* pred: header->predecessors()) {
        if (isLoopNode(LNF[pred], headerNode)) {
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
    modifiedCFG = true;
    return result;
}

static void eraseSingleValuePhiNodes(BasicBlock* BB) {
    SC_ASSERT(BB->numPredecessors() == 1, "");
    while (true) {
        auto* phi = dyncast<Phi*>(&BB->front());
        if (!phi) {
            break;
        }
        auto* arg = phi->argumentAt(0).value;
        replaceValue(phi, arg);
        BB->erase(phi);
    }
}

void GVNContext::addLandingPadToLoop(BasicBlock* header) {
    auto* headerNode = LNF[header];
    SC_ASSERT(headerNode->isProperLoop(), "");
    /// A loop is a do/while loop if it consists of only one node or if all
    /// paths from the header lead into the loop. Otherwise it is a while loop.
    bool const isDoWhileLoop =
        headerNode->children().empty() ||
        ranges::all_of(header->successors(), [&](BasicBlock const* succ) {
            return isLoopNode(LNF[succ], headerNode);
        });
    if (isDoWhileLoop) {
        addLandingPadToDoWhileLoop(header);
    }
    else {
        addLandingPadToWhileLoop(header);
    }
}

void GVNContext::addLandingPadToWhileLoop(BasicBlock* header) {
    auto [preHeader, outerPreds, innerPreds] = makeLoopSingleEntry(header);
    auto* headerNode = LNF[header];
    CloneValueMap cloneMap;
    auto* headerClone = ir::clone(ctx, header, cloneMap).release();
    LNF.addNode(header, headerClone);
    headerClone->setName(utl::strcat(header->name(), ".clone"));
    headerClone->removePredecessor(preHeader);
    auto* landingPad = new BasicBlock(ctx, "landingpad");
    auto [body, loopOutSucc] = [&]() -> std::array<BasicBlock*, 2> {
        SC_ASSERT(header->successors().size() <= 2, "");
        auto* succA = header->successors()[0];
        auto* succB = header->successors()[1];
        if (LNF[header]->parent() != LNF[succA]->parent()) {
            return { succA, succB };
        }
        return { succB, succA };
    }();
    if (loopOutSucc->numPredecessors() == 1) {
        eraseSingleValuePhiNodes(loopOutSucc);
        SC_ASSERT(loopOutSucc->singlePredecessor() == header, "");
        auto insertPoint = loopOutSucc->begin();
        for (auto& inst: *header) {
            auto loopOutUsers =
                inst.users() | ranges::views::filter([&](User* user) {
                    auto* inst = dyncast<Instruction*>(user);
                    return inst &&
                           !isLoopNode(LNF[inst->parent()], LNF[header]);
                }) |
                ranges::views::transform(
                    [](User* user) { return cast<Instruction*>(user); }) |
                ranges::to<utl::small_vector<Instruction*>>;
            if (loopOutUsers.empty()) {
                continue;
            }
            auto* phi =
                new Phi({ { header, &inst } }, std::string(inst.name()));
            loopOutSucc->insert(insertPoint, phi);
            for (auto* user: loopOutUsers) {
                user->updateOperand(&inst, phi);
            }
        }
    }
    if (body->predecessors().size() > 1) {
        auto* newEntry = addDistinctLoopEntry(header, body);
        headerClone->terminator()->updateTarget(body, newEntry);
        body = newEntry;
    }
    header->terminator()->updateTarget(body, landingPad);
    landingPad->addPredecessor(header);
    auto bodyInsertPoint = body->phiEnd();
    CloneValueMap phiMapHeaderToBody, phiMapHeaderCloneToBody;
    for (auto&& [phi, clonePhi]:
         ranges::views::zip(header->phiNodes(), headerClone->phiNodes()))
    {
        auto* bodyPhi = new Phi({ { landingPad, phi.operandOf(preHeader) },
                                  { headerClone, &clonePhi } },
                                std::string(phi.name()));
        body->insert(bodyInsertPoint, bodyPhi);
        phiMapHeaderToBody.add(&phi, bodyPhi);
        phiMapHeaderCloneToBody.add(&clonePhi, bodyPhi);
    }
    for (auto& clonePhi: headerClone->phiNodes()) {
        for (auto [index, arg]: clonePhi.arguments() | ranges::views::enumerate)
        {
            auto* instArg = dyncast<Instruction*>(arg.value);
            if (instArg && instArg->parent() == headerClone) {
                auto* bodyPhi = phiMapHeaderCloneToBody(instArg);
                clonePhi.setArgument(index, bodyPhi);
            }
        }
    }
    landingPad->pushBack(new Goto(ctx, body));
    body->setPredecessors(std::array{ landingPad, headerClone });
    for (auto* pred: innerPreds) {
        header->removePredecessor(pred);
        pred->terminator()->updateTarget(header, headerClone);
    }
    utl::small_vector<BasicBlock*> loopExitNodes = { header };
    LNF[header]->traversePreorder(
        [&, loopOutSucc = loopOutSucc](LNFNode const* node) {
        auto* BB = node->basicBlock();
        if (BB == header || BB == headerClone) {
            return;
        }
        for (auto* succ: BB->successors()) {
            if (!isLoopNode(LNF[succ], headerNode)) {
                SC_ASSERT(postDomInfo.domSet(succ).contains(loopOutSucc),
                          "If we exit here then loopOutSucc must post dominate "
                          "this node");
                if (succ == loopOutSucc) {
                    loopExitNodes.push_back(BB);
                    break;
                }
            }
        }
    });
    loopExitNodes.push_back(headerClone);
    auto* loopOut = new BasicBlock(ctx, "loop.out");
    loopOut->setPredecessors(loopExitNodes);
    for (auto* exitNode: loopExitNodes) {
        exitNode->terminator()->updateTarget(loopOutSucc, loopOut);
    }
    auto nonLoopPreds = loopOutSucc->predecessors() |
                        ranges::views::filter([&](BasicBlock* BB) {
                            return !ranges::contains(loopExitNodes, BB);
                        }) |
                        ranges::to<utl::small_vector<BasicBlock*>>;
    for (auto& phi: loopOutSucc->phiNodes()) {
        utl::small_vector<PhiMapping> phiArgs;
        for (auto* exitNode: loopExitNodes) {
            size_t index = phi.indexOf(exitNode);
            if (index != phi.argumentCount()) {
                phiArgs.push_back({ exitNode, phi.argumentAt(index).value });
            }
            else {
                SC_ASSERT(exitNode == headerClone, "");
                phiArgs.push_back(
                    { exitNode, cloneMap(phi.operandOf(header)) });
            }
        }
        auto* loPhi = new Phi(phiArgs, std::string(phi.name()));
        loopOut->pushBack(loPhi);
    }
    for (auto* exitNode: loopExitNodes) {
        if (ranges::contains(loopOutSucc->predecessors(), exitNode)) {
            loopOutSucc->removePredecessor(exitNode);
        }
    }
    loopOut->pushBack(new Goto(ctx, loopOutSucc));
    loopOutSucc->addPredecessor(loopOut);
    for (auto&& [loPhi, phi]:
         ranges::views::zip(loopOut->phiNodes(), loopOutSucc->phiNodes()))
    {
        phi.addArgument(loopOut, &loPhi);
    }
    for (auto* node: LNF[header]->children()) {
        if (node->basicBlock() == headerClone) {
            continue;
        }
        node->traversePreorder([&](LNFNode const* node) {
            auto* BB = node->basicBlock();
            for (auto& inst: *BB) {
                for (auto* operand: inst.operands()) {
                    auto* repl = phiMapHeaderToBody(operand);
                    if (repl != operand) {
                        inst.updateOperand(operand, repl);
                    }
                }
            }
        });
    }
    function.insert(header->next(), landingPad);
    function.insert(innerPreds.back()->next(), headerClone);
    function.insert(headerClone->next(), loopOut);
    modifiedCFG = true;
}

BasicBlock* GVNContext::addDistinctLoopEntry(BasicBlock* header,
                                             BasicBlock* entry) {
    auto* newEntry = new BasicBlock(ctx, "loop.entry");
    header->terminator()->updateTarget(entry, newEntry);
    newEntry->addPredecessor(header);
    newEntry->pushBack(new Goto(ctx, entry));
    entry->updatePredecessor(header, newEntry);
    function.insert(header->next(), newEntry);
    LNF.addNode(header, newEntry);
    return newEntry;
}

void GVNContext::addLandingPadToDoWhileLoop(BasicBlock* header) {
    makeLoopSingleEntry(header);
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
