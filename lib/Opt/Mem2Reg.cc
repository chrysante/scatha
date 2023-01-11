#include "Opt/Mem2Reg.h"

#include <map>

#include <utl/hashmap.hpp>
#include <utl/hashset.hpp>
#include <utl/vector.hpp>

#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/CFG.h"
#include "IR/Validate.h"
#include "Opt/ControlFlowPath.h"
#include "Opt/Common.h"

using namespace scatha;
using namespace opt;
using namespace ir;

namespace {

struct StoreContext {
    Store* store;
    BasicBlock* basicBlock;
    ssize_t positionInBB;
};

/// One context object will be created for every function we analyze.
struct Mem2RegContext {
    explicit Mem2RegContext(Context& ctx, Function& function): ctx(ctx), function(function) {}
    
    void analyze();
    
    /// ** Loads **
    
    void promoteLoads();
    bool promoteLoad(Load* load);
    utl::vector<ControlFlowPath> findRelevantStores(Load* load);
    Value* resolveStoredValue(utl::vector<ControlFlowPath> const& relevantStores);
    Value* generatePhi(utl::vector<ControlFlowPath> const& relevantStores);
    
    /// ** Stores **
    
    void evictDeadStores();
    /// Analyze a \p Store instruction and return true if this is a store to local variable that is not being read anymore.
    bool isDead(Store const* store);
    
    /// ** Allocas **
    void evictDeadAllocas();
    /// Analyze a \p Alloca instruction and return true it's memory is not being read.
    bool isDead(Alloca const* address);
    
    /// ** Generic methods **
    
    void gather();
    
    void removeDuplicatePhis();
    
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

void opt::mem2Reg(ir::Context& context, ir::Module& mod) {
    for (auto& function: mod.functions()) {
        Mem2RegContext ctx(context, function);
        ctx.analyze();
    }
    ir::assertInvariants(context, mod);
}

void Mem2RegContext::analyze() {
    gather();
    promoteLoads();
    evictDeadStores();
    evictDeadAllocas();
    removeDuplicatePhis();
}

/// MARK: Loads

void Mem2RegContext::promoteLoads() {
    for (auto itr = loads.begin(); itr != loads.end(); ) {
        auto* load = *itr;
        if (!promoteLoad(load)) {
            ++itr;
            continue;
        }
        load->parent()->instructions.erase(load);
        itr = loads.erase(itr);
    }
}

bool Mem2RegContext::promoteLoad(Load* load) {
    auto* const address = load->address();
    if (load->users().empty()) {
        /// If this load is not being used we can just erase it.
        return true;
    }
    if (!allocas.contains(address)) {
        /// It is rather cheap to early out here, extend this later to also analyze non-locally allocated memory.
        /// Right now we do this, so we can be sure that every load is preceeded by a store in every code path or we
        /// have UB so we can just assume we have a store.
        return false;
    }
    auto const relevantStores = findRelevantStores(load);
    auto* value = resolveStoredValue(relevantStores);
    if (!value) {
        return false;
    }
    replaceValue(load, value);
    return true;
}

Value* Mem2RegContext::resolveStoredValue(utl::vector<ControlFlowPath> const& relevantStores) {
    switch (relevantStores.size()) {
    case 0:
        return nullptr;
    case 1: {
        auto& path = relevantStores.front();
        auto* store = const_cast<Store*>(cast<Store const*>(&path.back()));
        return store->source();
    }
    default: {
        /// This is a bit harder
        return generatePhi(relevantStores);
    }
    }
}

Value* Mem2RegContext::generatePhi(utl::vector<scatha::opt::ControlFlowPath> const& relevantStores) {
    BasicBlock* const basicBlock = const_cast<BasicBlock*>(relevantStores.front().basicBlocks().front());
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
                return resolveStoredValue(newPaths);
            }
        }();
        phiArgs.push_back({ const_cast<BasicBlock*>(pred), value });
    }
    if (phiArgs.size() == 1) {
        return phiArgs.front().value;
    }
    auto* phi = new Phi(phiArgs, ctx.uniqueName(&function, "promoted-load"));
    phi->set_parent(basicBlock);
    basicBlock->instructions.push_front(phi);
    return phi;
}

static bool containsTwice(std::span<BasicBlock const* const> path, BasicBlock const* bb) {
    int count = 0;
    for (auto& pathBB: path) {
        count += pathBB == bb;
        if (count == 2) { return true; }
    }
    return false;
};

utl::vector<ControlFlowPath> Mem2RegContext::findRelevantStores(Load* load) {
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

/// MARK: Stores

void Mem2RegContext::evictDeadStores() {
    for (auto& bb: function.basicBlocks()) {
        for (auto instItr = bb.instructions.begin(); instItr != bb.instructions.end(); ) {
            auto* const store = dyncast<Store*>(instItr.to_address());
            if (!store) { ++instItr; continue; }
            bool const canErase = isDead(store);
            if (canErase) {
                instItr = bb.instructions.erase(instItr);
                continue;
            }
            ++instItr;
        }
    }
}

bool Mem2RegContext::isDead(Store const* store) {
    Value const* const address = store->dest();
    if (!allocas.contains(address)) {
        /// We can only guarantee that this store is dead if the memory was locally allocated by this function.
        return false;
    }
    for (auto& load: loads) {
        if (load->address() != address) { continue; }
        bool const loadIsReachable = isReachable(store, load);
        if (loadIsReachable) {
            return false;
        }
    }
    return true;
}

/// MARK: Allocas

void Mem2RegContext::evictDeadAllocas() {
    for (auto* allocaInst: allocas) {
        bool const canErase = isDead(allocaInst);
        if (canErase) {
            allocaInst->parent()->instructions.erase(allocaInst);
            continue;
        }
    }
}

bool Mem2RegContext::isDead(Alloca const* address) {
    /// See if there is any load from this address
    for (auto* load: loads) {
        /// \Warning What about GEPs, see \p Store case.
        if (load->address() != address) { continue; }
        return false;
    }
    return true;
}

/// MARK: Generic methods

void Mem2RegContext::gather() {
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

void Mem2RegContext::removeDuplicatePhis() {
    for (auto& bb: function.basicBlocks()) {
        utl::small_vector<Phi*> phis;
        for (auto& inst: bb.instructions) {
            auto* phi = dyncast<Phi*>(&inst);
            if (!phi) {
                break;
            }
            phis.push_back(phi);
        }
        for (auto iPhi = phis.begin(); iPhi != phis.end(); ++iPhi) {
            for (auto jPhi = iPhi + 1; jPhi != phis.end(); ) {
                if (!compareEqual(*iPhi, *jPhi)) {
                    ++jPhi;
                    continue;
                }
                replaceValue(*jPhi, *iPhi);
                bb.instructions.erase(*jPhi);
                jPhi = phis.erase(jPhi);
            }
        }
    }
}
