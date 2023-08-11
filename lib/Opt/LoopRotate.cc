#include "Opt/Passes.h"

#include <utl/hashtable.hpp>
#include <utl/strcat.hpp>
#include <utl/vector.hpp>

#include "IR/CFG.h"
#include "IR/Clone.h"
#include "IR/Dominance.h"
#include "IR/Loop.h"
#include "IR/Validate.h"
#include "Opt/Common.h"
#include "Opt/PassRegistry.h"

using namespace scatha;
using namespace opt;
using namespace ir;

SC_REGISTER_PASS(opt::rotateLoops, "rotateloops");

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

    void rotateLoop(BasicBlock* header);

    void rotateWhileLoop(BasicBlock* guard,
                         SingleLoopEntryResult const& seResult);
};

} // namespace

bool opt::rotateLoops(Context& ctx, Function& function) {
    LRContext lrContext(ctx, function);
    bool result = lrContext.run();
    assertInvariants(ctx, function);
    return result;
}

bool LRContext::run() {
    for (auto* root: LNF.roots()) {
        root->traversePreorder([&](LNFNode const* node) {
            if (node->isProperLoop()) {
                rotateLoop(node->basicBlock());
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

void LRContext::rotateLoop(BasicBlock* header) {
    auto* headerNode = LNF[header];
    SC_ASSERT(headerNode->isProperLoop(), "");
    auto seResult = makeLoopSingleEntry(header);
    /// A loop is a do/while loop if it consists of only one node or if all
    /// paths from the header lead into the loop. Otherwise it is a while loop.
    bool const isDoWhileLoop =
        headerNode->children().empty() ||
        ranges::all_of(header->successors(), [&](BasicBlock const* succ) {
            return isLoopNode(LNF[succ], headerNode);
        });
    if (!isDoWhileLoop) {
        rotateWhileLoop(header, seResult);
    }
}

void LRContext::rotateWhileLoop(BasicBlock* header,
                                SingleLoopEntryResult const& seResult) {
    auto& [preHeader, outerPreds, innerPreds] = seResult;

    /// Make footer
    CloneValueMap phiMapGuardToFooter;
    auto* footer = ir::clone(ctx, header, phiMapGuardToFooter).release();
    function.insert(innerPreds.back()->next(), footer);
    LNF.addNode(header, footer);
    footer->setName("loop.footer");
    footer->removePredecessor(preHeader);

    /// Rename header to guard
    auto* guard = header;
    guard->setName("loop.guard");

    /// Get entry and skip block
    auto [entry, skipBlock] = [&] {
        SC_ASSERT(guard->successors().size() <= 2, "");
        auto* succA = guard->successors()[0];
        auto* succB = guard->successors()[1];
        if (LNF[guard]->parent() != LNF[succA]->parent()) {
            return std::pair{ succA, succB };
        }
        return std::pair{ succB, succA };
    }();

    /// If the skip block only has one predecessor, we add explicit phi nodes
    /// with one argument because later in the function the footer will also
    /// become a predecessor of the skip block
    if (skipBlock->numPredecessors() == 1) {
        eraseSingleValuePhiNodes(skipBlock);
        SC_ASSERT(skipBlock->singlePredecessor() == guard, "");
        auto insertPoint = skipBlock->begin();
        for (auto& inst: *guard) {
            auto loopOutUsers =
                inst.users() | ranges::views::filter([&](User* user) {
                    auto* inst = dyncast<Instruction*>(user);
                    return inst && !isLoopNode(LNF[inst->parent()], LNF[guard]);
                }) |
                ranges::views::transform(
                    [](User* user) { return cast<Instruction*>(user); }) |
                ranges::to<utl::small_vector<Instruction*>>;
            if (loopOutUsers.empty()) {
                continue;
            }
            auto* phi = new Phi({ { guard, &inst } }, std::string(inst.name()));
            skipBlock->insert(insertPoint, phi);
            for (auto* user: loopOutUsers) {
                user->updateOperand(&inst, phi);
            }
        }
    }

    /// If the entry block has multiple predecessors, i.e. it is itself a loop
    /// header, then we add a seperate entry block that can become the new loop
    /// header
    if (entry->predecessors().size() > 1) {
        auto* newEntry = new BasicBlock(ctx, "loop.entry");
        guard->terminator()->updateTarget(entry, newEntry);
        newEntry->addPredecessor(guard);
        newEntry->pushBack(new Goto(ctx, entry));
        entry->updatePredecessor(guard, newEntry);
        function.insert(guard->next(), newEntry);
        LNF.addNode(guard, newEntry);
        footer->terminator()->updateTarget(entry, newEntry);
        entry = newEntry;
    }

    /// Add landing pad
    auto* landingPad = new BasicBlock(ctx, "loop.landingpad");
    function.insert(guard->next(), landingPad);
    guard->terminator()->updateTarget(entry, landingPad);
    landingPad->addPredecessor(guard);
    landingPad->pushBack(new Goto(ctx, entry));
    entry->setPredecessors(std::array{ landingPad, footer });
    for (auto* pred: innerPreds) {
        guard->removePredecessor(pred);
        pred->terminator()->updateTarget(guard, footer);
    }

    /// Rewrite phi nodes of guard, entry and footer
    auto bodyInsertPoint = entry->phiEnd();
    CloneValueMap phiMapGuardToEntry, phiMapFooterToEntry;
    for (auto&& [guardPhi, footerPhi]:
         ranges::views::zip(guard->phiNodes(), footer->phiNodes()))
    {
        auto* entryPhi =
            new Phi({ { landingPad, guardPhi.operandOf(preHeader) },
                      { footer, &footerPhi } },
                    std::string(guardPhi.name()));
        entry->insert(bodyInsertPoint, entryPhi);
        phiMapGuardToEntry.add(&guardPhi, entryPhi);
        phiMapFooterToEntry.add(&footerPhi, entryPhi);
    }
    for (auto& footerPhi: footer->phiNodes()) {
        for (auto [index, arg]:
             footerPhi.arguments() | ranges::views::enumerate)
        {
            auto* instArg = dyncast<Instruction*>(arg.value);
            if (instArg && instArg->parent() == footer) {
                auto* entryPhi = phiMapFooterToEntry(instArg);
                footerPhi.setArgument(index, entryPhi);
            }
        }
    }

    /// Replace uses of phi nodes in the guard block by phi nodes in the entry
    /// block
    for (auto* node: LNF[guard]->children()) {
        if (node->basicBlock() == footer) {
            continue;
        }
        node->traversePreorder([&](LNFNode const* node) {
            for (auto& inst: *node->basicBlock()) {
                for (auto* operand: inst.operands()) {
                    auto* repl = phiMapGuardToEntry(operand);
                    if (repl != operand) {
                        inst.updateOperand(operand, repl);
                    }
                }
            }
        });
    }

    /// Gather exiting blocks
    utl::small_vector<BasicBlock*> exitingBlocks = { guard };
    auto* headerNode = LNF[header];
    LNF[guard]->traversePreorder(
        [&, skipBlock = skipBlock](LNFNode const* node) {
        auto* BB = node->basicBlock();
        if (BB == guard || BB == footer) {
            return;
        }
        for (auto* succ: BB->successors()) {
            if (!isLoopNode(LNF[succ], headerNode)) {
                SC_ASSERT(postDomInfo.domSet(succ).contains(skipBlock),
                          "If we exit here then skipBlock must post dominate "
                          "this node");
                if (succ == skipBlock) {
                    exitingBlocks.push_back(BB);
                    break;
                }
            }
        }
    });
    exitingBlocks.push_back(footer);

    /// Add new exit block
    auto* exitBlock = new BasicBlock(ctx, "loop.exit");
    function.insert(footer->next(), exitBlock);
    exitBlock->setPredecessors(exitingBlocks);
    for (auto* exitingBlock: exitingBlocks) {
        exitingBlock->terminator()->updateTarget(skipBlock, exitBlock);
    }

    /// Add phi nodes to exit block
    for (auto& phi: skipBlock->phiNodes()) {
        utl::small_vector<PhiMapping> phiArgs;
        for (auto* exitingBlock: exitingBlocks) {
            size_t index = phi.indexOf(exitingBlock);
            if (index != phi.argumentCount()) {
                phiArgs.push_back(
                    { exitingBlock, phi.argumentAt(index).value });
            }
            else {
                SC_ASSERT(exitingBlock == footer, "");
                phiArgs.push_back(
                    { exitingBlock,
                      phiMapGuardToFooter(phi.operandOf(guard)) });
            }
        }
        auto* exitPhi = new Phi(phiArgs, std::string(phi.name()));
        exitBlock->pushBack(exitPhi);
    }

    /// Rewire CFG wrt exit block
    for (auto* exitingBlock: exitingBlocks) {
        if (ranges::contains(skipBlock->predecessors(), exitingBlock)) {
            skipBlock->removePredecessor(exitingBlock);
        }
    }
    exitBlock->pushBack(new Goto(ctx, skipBlock));
    skipBlock->addPredecessor(exitBlock);

    /// Update phi nodes of skip block
    for (auto&& [exitPhi, skipPhi]:
         ranges::views::zip(exitBlock->phiNodes(), skipBlock->phiNodes()))
    {
        skipPhi.addArgument(exitBlock, &exitPhi);
    }

    modified = true;
}
