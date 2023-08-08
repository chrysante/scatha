#include "Opt/LoopRotate.h"

#include <utl/hashtable.hpp>
#include <utl/strcat.hpp>
#include <utl/vector.hpp>

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

struct SingleLoopEntryResult {
    BasicBlock* preHeader = nullptr;
    utl::small_vector<BasicBlock*> outerPreds, innerPreds;
};

struct LRContext {
    Context& ctx;
    Function& function;
    LoopNestingForest& LNF;
    DominanceInfo const& postDomInfo;

    bool modified = false;

    LRContext(Context& context, Function& function):
        ctx(context),
        function(function),
        LNF(const_cast<LoopNestingForest&>(function.getOrComputeLNF())),
        postDomInfo(function.getOrComputePostDomInfo()) {}

    bool run();

    SingleLoopEntryResult makeLoopSingleEntry(BasicBlock* header);

    void addLandingPadToLoop(BasicBlock* header);

    void addPhiNodesToSkipBlock(BasicBlock* skipBlock, BasicBlock* header);

    void addLandingPadToWhileLoop(BasicBlock* header);

    std::pair<BasicBlock*, BasicBlock*> getBodyAndSkipBlock(BasicBlock* header);

    /// Add a node as the loop entry node with only one predecessor. This is
    /// called when the loop entry node has multiple predecessors.
    BasicBlock* addDistinctLoopEntry(BasicBlock* header, BasicBlock* entry);

    void addLandingPadToDoWhileLoop(BasicBlock* header);
};

} // namespace

bool opt::rotateWhileLoops(Context& ctx, Function& function) {
    LRContext lrContext(ctx, function);
    bool result = lrContext.run();
    assertInvariants(ctx, function);
    return result;
}

bool LRContext::run() {
    for (auto* root: LNF.roots()) {
        root->traversePreorder([&](LNFNode const* node) {
            if (node->isProperLoop()) {
                addLandingPadToLoop(node->basicBlock());
            }
        });
    }
    if (modified) {
        function.invalidateCFGInfo();
    }
    return modified;
}

static bool isLoopNode(LNFNode const* node, LNFNode const* header) {
    while (node != nullptr) {
        if (node == header) {
            return true;
        }
        node = node->parent();
    }
    return false;
}

SingleLoopEntryResult LRContext::makeLoopSingleEntry(BasicBlock* header) {
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
        auto* phPhi = new Phi(newPhiArgs, std::string(phi.name()));
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
    modified = true;
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

void LRContext::addLandingPadToLoop(BasicBlock* header) {
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

void LRContext::addLandingPadToWhileLoop(BasicBlock* header) {
    auto [preHeader, outerPreds, innerPreds] = makeLoopSingleEntry(header);
    auto* headerNode = LNF[header];
    CloneValueMap cloneMap;
    auto* headerClone = ir::clone(ctx, header, cloneMap).release();
    LNF.addNode(header, headerClone);
    headerClone->setName(utl::strcat(header->name(), ".clone"));
    headerClone->removePredecessor(preHeader);
    auto* landingPad = new BasicBlock(ctx, "landingpad");
    auto [body, skipBlock] = getBodyAndSkipBlock(header);
    if (skipBlock->numPredecessors() == 1) {
        addPhiNodesToSkipBlock(skipBlock, header);
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
        [&, skipBlock = skipBlock](LNFNode const* node) {
        auto* BB = node->basicBlock();
        if (BB == header || BB == headerClone) {
            return;
        }
        for (auto* succ: BB->successors()) {
            if (!isLoopNode(LNF[succ], headerNode)) {
                SC_ASSERT(postDomInfo.domSet(succ).contains(skipBlock),
                          "If we exit here then skipBlock must post dominate "
                          "this node");
                if (succ == skipBlock) {
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
        exitNode->terminator()->updateTarget(skipBlock, loopOut);
    }
    auto nonLoopPreds = skipBlock->predecessors() |
                        ranges::views::filter([&](BasicBlock* BB) {
                            return !ranges::contains(loopExitNodes, BB);
                        }) |
                        ranges::to<utl::small_vector<BasicBlock*>>;
    for (auto& phi: skipBlock->phiNodes()) {
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
        if (ranges::contains(skipBlock->predecessors(), exitNode)) {
            skipBlock->removePredecessor(exitNode);
        }
    }
    loopOut->pushBack(new Goto(ctx, skipBlock));
    skipBlock->addPredecessor(loopOut);
    for (auto&& [loPhi, phi]:
         ranges::views::zip(loopOut->phiNodes(), skipBlock->phiNodes()))
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
    modified = true;
}

std::pair<BasicBlock*, BasicBlock*> LRContext::getBodyAndSkipBlock(
    BasicBlock* header) {
    SC_ASSERT(header->successors().size() <= 2, "");
    auto* succA = header->successors()[0];
    auto* succB = header->successors()[1];
    if (LNF[header]->parent() != LNF[succA]->parent()) {
        return { succA, succB };
    }
    return { succB, succA };
}

void LRContext::addPhiNodesToSkipBlock(BasicBlock* skipBlock,
                                       BasicBlock* header) {
    eraseSingleValuePhiNodes(skipBlock);
    SC_ASSERT(skipBlock->singlePredecessor() == header, "");
    auto insertPoint = skipBlock->begin();
    for (auto& inst: *header) {
        auto loopOutUsers =
            inst.users() | ranges::views::filter([&](User* user) {
                auto* inst = dyncast<Instruction*>(user);
                return inst && !isLoopNode(LNF[inst->parent()], LNF[header]);
            }) |
            ranges::views::transform(
                [](User* user) { return cast<Instruction*>(user); }) |
            ranges::to<utl::small_vector<Instruction*>>;
        if (loopOutUsers.empty()) {
            continue;
        }
        auto* phi = new Phi({ { header, &inst } }, std::string(inst.name()));
        skipBlock->insert(insertPoint, phi);
        for (auto* user: loopOutUsers) {
            user->updateOperand(&inst, phi);
        }
    }
}

BasicBlock* LRContext::addDistinctLoopEntry(BasicBlock* header,
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

void LRContext::addLandingPadToDoWhileLoop(BasicBlock* header) {
    makeLoopSingleEntry(header);
}
