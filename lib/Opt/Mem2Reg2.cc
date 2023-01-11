#include "Opt/Mem2Reg2.h"

#include <map>

#include <utl/hashmap.hpp>
#include <utl/hashset.hpp>
#include <utl/vector.hpp>

#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/CFG.h"
#include "Opt/ControlFlowPath.h"
#include "Opt/Common.h"

#include <iostream>
#include "IR/Print.h"

using namespace scatha;
using namespace opt;
using namespace ir;

namespace {

struct StoreContext {
    Store* store;
    BasicBlock* basicBlock;
    ssize_t positionInBB;
};

struct Ctx {
    explicit Ctx(Context& ctx, Function& function): ctx(ctx), function(function) {}
    
    void analyze();
    
    void analyze(Load* load);
    utl::vector<ControlFlowPath> findRelevantStores(Load* load);
    Phi *generatePhi(BasicBlock* basicBlock, utl::vector<ControlFlowPath> const& relevantStores);
    void gather();
    
    Context& ctx;
    Function& function;
    
    /// Maps addresses to stores
    utl::hashmap<Value*, utl::small_vector<StoreContext>> stores;
    /// Table of locally allocated memory
    utl::hashset<Alloca*> allocas;
    /// List of all load instructions in the current function
    utl::small_vector<Load*> loads;
};

} // namespace

void opt::mem2Reg2(ir::Context& context, ir::Module& mod) {
    for (auto& function: mod.functions()) {
        Ctx ctx(context, function);
        ctx.analyze();
    }
}

void Ctx::analyze() {
    gather();
    for (auto* load: loads) {
        analyze(load);
    }
}

static bool containsTwice(std::span<BasicBlock const* const> path, BasicBlock const* bb) {
    int count = 0;
    for (auto& pathBB: path) {
        count += pathBB == bb;
        if (count == 2) { return true; }
    }
    return false;
};

Phi* Ctx::generatePhi(BasicBlock* basicBlock, utl::vector<scatha::opt::ControlFlowPath> const& relevantStores) {
    assert(relevantStores.size() >= basicBlock->predecessors.size()); // We need to have at least as many preceeding stores as predecessors to our BB.
                                                                      // Otherwise we have UB to to read of uninitialized memory.
    /// Mapping predecessors to incoming paths
    utl::hashmap<BasicBlock const*, utl::small_vector<ControlFlowPath const*>> map;
    for (auto& path: relevantStores) {
        assert(path.basicBlocks().size() > 1); // Otherwise we should not be here. We should be in case 1 because search should have stopped at the preceeding store in our BB.
        map[path.basicBlocks()[1]].push_back(&path);
    }
    utl::small_vector<PhiMapping> phiArgs;
    for (auto kv: map) {
        auto& pred = kv.first;
        auto& paths = kv.second;
        auto* const value = [&]() -> Value* {
            if (paths.size() == 1) {
                auto& path = paths.front();
                auto* store = cast<Store*>(const_cast<Instruction*>(&path->back()));
                return store->source();
            }
            /// Here we have more than one path which all enter our BB through the same predecessor.
            for (size_t i = 0; ; ++i) {
                /// We shave off the first basic block from each path
                utl::vector<scatha::opt::ControlFlowPath> newPaths(utl::transform(paths, [](auto* p) { return *p; }));
                for (auto& path: newPaths) { path.basicBlocks().erase(path.basicBlocks().begin()); }
                auto* pred = newPaths.front().basicBlocks().front();
                return generatePhi(const_cast<BasicBlock*>(pred), newPaths);
            }
        }();
        phiArgs.push_back({ const_cast<BasicBlock*>(pred), value });
    }
    auto* phi = new Phi(phiArgs, ctx.uniqueName(&function, "promoted-load"));
    phi->set_parent(basicBlock);
    basicBlock->instructions.push_front(phi);
    return phi;
}

void Ctx::analyze(Load* load) {
    auto* const address = load->address();
    auto* const basicBlock = load->parent();
    if (load->users().empty()) {
        /// If this load is not being used we can just erase it.
        basicBlock->instructions.erase(load);
        return;
    }
    if (!allocas.contains(address)) {
        /// It is rather cheap to early out here, extend this later to also analyze non-locally allocated memory.
        /// Right now we do this, so we can be sure that every load is preceeded by a store in every code path or we
        /// have UB so we can just assume we have a store.
        return;
    }
    auto relevantStores = findRelevantStores(load);
    switch (relevantStores.size()) {
    case 0:
        return;
    case 1: {
        auto& path = relevantStores.front();
        auto* store = const_cast<Store*>(cast<Store const*>(&path.back()));
        replaceValue(load, store->source());
        basicBlock->instructions.erase(load);
        break;
    }
    default: {
        /// This is a bit harder
        scatha::ir::Phi *const phi = generatePhi(basicBlock, relevantStores);
        replaceValue(load, phi);
        auto itr = basicBlock->instructions.erase(load);
        break;
    }
    }
}

utl::vector<ControlFlowPath> Ctx::findRelevantStores(Load* load) {
    using Map = std::map<size_t, ControlFlowPath>;
    Map paths;
    size_t id = 0;
    auto* const address = load->address();
    auto const& relevantStores = stores[address];
    auto search = [&](Map::iterator currentPathItr, BasicBlock* currentNode, auto& search) mutable {
        auto& currentPath = currentPathItr->second;
        /// We allow nodes occuring twice to allow cycles, but we only want to traverse the cycle once.
        if (containsTwice(currentPath.basicBlocks(), currentNode)) {
            paths.erase(currentPathItr);
            return;
        }
        /// Add the current node to the path
        opt::internal::addBasicBlock(currentPath, currentNode);
        /// Search the current node for a store instruction
        ssize_t weight = std::numeric_limits<ssize_t>::min();
        Store* relevantStore = nullptr;
        for (auto& c: relevantStores) {
            if (c.basicBlock != currentNode) {
                continue;
            }
            if (c.positionInBB > weight) {
                relevantStore = c.store;
            }
        }
        if (relevantStore != nullptr && (currentPath.basicBlocks().size() > 1 || preceeds(relevantStore, load))) {
            /// We have found a store to our address. We don't need to search further in this path.
            currentPath.setBack(relevantStore);
            return;
        }
        switch (currentNode->predecessors.size()) {
        case 0:
            paths.erase(currentPathItr);
            break;
        case 1:
            search(currentPathItr, currentNode->predecessors.front(), search);
            break;
        default:
            auto cpCopy = currentPath;
            search(currentPathItr, currentNode->predecessors.front(), search);
            for (size_t i = 1; i < currentNode->predecessors.size(); ++i) {
                auto [itr, success] = paths.insert({ id++, cpCopy });
                SC_ASSERT(success, "");
                search(itr, currentNode->predecessors[i], search);
            }
            break;
        }
    };
    auto [itr, success] = paths.insert({ id++, ControlFlowPath(load, nullptr) });
    search(itr, load->parent(), search);
    return utl::vector<ControlFlowPath>(utl::transform(paths, [](auto& p) { return p.second; }));
}

void Ctx::gather() {
    for (auto& bb: utl::reverse(function.basicBlocks())) {
        for (ssize_t positionInBB = 0; auto& inst: utl::reverse(bb.instructions)) {
            --positionInBB;
            visit(inst, utl::overload { // clang-format off
                [&](Alloca& allocaInst) {
                    allocas.insert(&allocaInst);
                },
                [&](Store& store) {
                    auto* const dest = store.dest();
                    stores[dest].push_back({ &store, store.parent(), positionInBB });
                },
                [&](Load& load) {
                    loads.push_back(&load);
                },
                [](auto&) {}
            }); // clang-format on
        }
    }
}

