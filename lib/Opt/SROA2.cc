#include "Opt/Passes.h"

#include <set>
#include <queue>

#include <range/v3/algorithm.hpp>
#include <utl/vector.hpp>
#include <utl/strcat.hpp>
#include <utl/hashtable.hpp>

#include "Common/Allocator.h"
#include "Common/Ranges.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Type.h"
#include "IR/Clone.h"
#include "IR/Dominance.h"
#include "IR/Validate.h"
#include "Opt/PassRegistry.h"
#include "Opt/Common.h"

#include <iostream>

using namespace scatha;
using namespace ir;
using namespace opt;

SC_REGISTER_PASS(opt::sroa2, "sroa2");

namespace {

struct Variable {
    Context& ctx;
    Function& function;
    Alloca* baseAlloca;
    
    utl::hashset<Load*> loads;
    utl::hashset<Store*> stores;
    utl::hashset<GetElementPointer*> geps;
    utl::hashset<Phi*> phis;
    utl::hashset<Instruction*> transitivePointers;
    utl::hashmap<Instruction*, Phi*> assocPhis;
    
    Variable(Context& ctx, Function& function, Alloca* baseAlloca):
        ctx(ctx), function(function), baseAlloca(baseAlloca) {}
    
    Phi* getAssocPhi(Instruction* inst) const {
        auto itr = assocPhis.find(inst);
        if (itr != assocPhis.end()) { return itr->second; }
        return nullptr;
    }
    
    void memorize(Instruction* inst) {
        // clang-format off
        SC_MATCH (*inst) {
            [&](Load& load) { loads.insert(&load); },
            [&](Store& store) { stores.insert(&store); },
            [&](GetElementPointer& gep) { geps.insert(&gep); },
            [&](Phi& phi) { phis.insert(&phi); },
            [&](Instruction& inst) { SC_UNREACHABLE(); },
        }; // clang-format on
    }
    
    void forget(Instruction* inst) {
        // clang-format off
        SC_MATCH (*inst) {
            [&](Load& load) { loads.erase(&load); },
            [&](Store& store) { stores.erase(&store); },
            [&](GetElementPointer& gep) { geps.erase(&gep); },
            [&](Phi& phi) { phis.erase(&phi); },
            [&](Instruction& inst) { SC_UNREACHABLE(); },
        }; // clang-format on
        assocPhis.erase(inst);
    }
    
    bool run();
    
    /// # Analysis
    bool analyze(Instruction* inst);
    bool analyzeUsers(Instruction* inst);
    
    bool analyzeImpl(Instruction* inst);
    bool analyzeImpl(Alloca* base);
    bool analyzeImpl(Load* load);
    bool analyzeImpl(Store* store);
    bool analyzeImpl(GetElementPointer* gep);
    bool analyzeImpl(Phi* phi);
    
    bool addPointer(Instruction* inst);
    
    /// #
    void rewritePhis();
    
    Instruction* copyInstruction(Instruction* inst, BasicBlock* dest);
};

} // namespace

bool opt::sroa2(Context& ctx, Function& function) {
    auto worklist = function.entry() | Filter<Alloca> | TakeAddress | ToSmallVector<>;
    for (auto* baseAlloca: worklist) {
        Variable var(ctx, function, baseAlloca);
        if (!var.run()) {
            continue;
        }
    }
    return false;
}

bool Variable::run() {
    if (!analyze(baseAlloca)) {
        return false;
    }
    rewritePhis();
    return true;
}

bool Variable::analyze(Instruction* inst) {
    return visit(*inst, [&](auto& inst) { return analyzeImpl(&inst); });
}

bool Variable::analyzeUsers(Instruction* inst) {
    return ranges::all_of(inst->users(), [&](auto* user) {
        if (Phi* phi = nullptr;
            (phi = dyncast<Phi*>(inst)) || (phi = getAssocPhi(inst)))
        {
            assocPhis[user] = phi;
        }
        return analyze(user);
    });
}

bool Variable::analyzeImpl(Instruction* inst) {
    return false;
}

bool Variable::analyzeImpl(Alloca* base) {
    addPointer(base);
    return analyzeUsers(base);
}

bool Variable::analyzeImpl(Load* load) {
    memorize(load);
    return true;
}

bool Variable::analyzeImpl(Store* store) {
    /// If we store any pointer into the alloca it escapes our analysis
    if (transitivePointers.contains(store->value())) {
        return false;
    }
    memorize(store);
    return true;
}

bool Variable::analyzeImpl(GetElementPointer* gep) {
    addPointer(gep);
    memorize(gep);
    return analyzeUsers(gep);
}

bool Variable::analyzeImpl(Phi* phi) {
    addPointer(phi);
    memorize(phi);
    return analyzeUsers(phi);
}

bool Variable::addPointer(Instruction* inst) {
    return transitivePointers.insert(inst).second;
}

void Variable::rewritePhis() {
    if (phis.empty()) {
        return;
        
    }
    splitCriticalEdges(ctx, function);
    auto const& postDomTree = function.getOrComputePostDomInfo().domTree();
    if (postDomTree.empty()) {
        return;
    }
    utl::small_vector<Instruction*> toErase;
    utl::hashmap<std::pair<BasicBlock*, Instruction*>, Instruction*> toCopyMap;
    postDomTree.root()->traversePreorder([&](DomTree::Node const* node) {
        auto* BB = node->basicBlock();
        for (auto& inst: *BB | Filter<Load, Store, GetElementPointer>) {
            auto* phi = getAssocPhi(&inst);
            if (!phi) {
                continue;
            }
            /// We look at all instructions that have an associated phi node.
            /// We make copies of the instructions in each of the predecessor
            /// blocks of the phi and phi the instructions together
            utl::small_vector<PhiMapping> newPhiArgs;
            for (auto [pred, value]: phi->arguments()) {
                auto* copy = copyInstruction(&inst, pred);
                toCopyMap[{ pred, &inst }] = copy;
                for (auto [index, operand]: copy->operands() | ranges::views::enumerate) {
                    if (operand == phi) {
                        copy->setOperand(index, value);
                        continue;
                    }
                    auto itr = toCopyMap.find({ pred, dyncast<Instruction*>(operand) });
                    if (itr != toCopyMap.end()) {
                        copy->setOperand(index, itr->second);
                    }
                }
                newPhiArgs.push_back({ pred, copy });
                memorize(copy);
                if (auto* assocPhi = getAssocPhi(dyncast<Instruction*>(value))) {
                    assocPhis[copy] = assocPhi;
                }
                if (auto* assocPhi = dyncast<Phi*>(value)) {
                    assocPhis[copy] = assocPhi;
                }
            }
            if (inst.isUsed()) {
                auto* newPhi = new Phi(newPhiArgs, utl::strcat(inst.name(), ".phi"));
                phi->parent()->insert(phi, newPhi);
                inst.replaceAllUsesWith(newPhi);
            }
            toErase.push_back(&inst);
        }
    });
    for (auto* inst: toErase) {
        forget(inst);
        inst->parent()->erase(inst);
    }
    for (auto* phi: phis) {
        SC_ASSERT(ranges::all_of(phi->users(), [](auto* user) { return isa<Phi>(user); }),
                  "All users of the phis must be other phis at this point");
        phi->parent()->erase(phi);
    }
}

Instruction* Variable::copyInstruction(Instruction* inst, BasicBlock* dest) {
    auto* copy = clone(ctx, inst).release();
    dest->insert(dest->terminator(), copy);
    return copy;
}
