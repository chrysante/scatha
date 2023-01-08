#include "Opt/Mem2Reg.h"

#include <map>

#include <utl/hashmap.hpp>
#include <utl/vector.hpp>

#include "IR/CFG.h"
#include "IR/Module.h"

using namespace scatha;
using namespace ir;

namespace {

struct Ctx {
    Ctx(Module& mod): mod(mod) {}
    
    void run();
    
    /// ** Loads **
    
    void promoteLoads(Function& function);
    
    /// Analyze a \p Load instruction and if possible return a \p Phi instruction that can replace it.
    Phi* promoteLoad(Load* load);
    
    struct RelevantStoreResultPair {
        Store* store;
        size_t distance;
    };
    
    utl::hashmap<BasicBlock*, RelevantStoreResultPair> findRelevantStores(std::span<Store* const> stores, Load* load);
    
    Store* findLastStoreToAddress(std::span<BasicBlock* const> path, Value const* address);
    
    /// ** Stores **
    
    void evictDeadStores(Function& function);
    
    /// Analyze a \p Store instruction and return true if this is a store to local variable that is not being read anymore.
    bool isDead(Store const* store);
    
    /// Check wether this function contains an \p alloca instruction that allocated the memory of \p address
    bool isLocalToFunction(Value const* address, Function const& function);
   
    /// ** Allocas **
    
    
    
    /// ** Generic methods **
    
    utl::vector<utl::small_vector<BasicBlock*>> findAllPaths(BasicBlock* origin, BasicBlock* dest);
    
    /// Analyze wether there is a control flow path from \p from to \p to
    bool isReachable(Instruction const* from, Instruction const* to);
    
    Module& mod;
};

} // namespace

void opt::mem2Reg(ir::Module& mod) {
    Ctx ctx(mod);
    ctx.run();
}

void Ctx::run() {
    for (auto& function: mod.functions()) {
        promoteLoads(function);
        evictDeadStores(function);
    }
}

/// ** Loads **

void Ctx::promoteLoads(Function& function) {
    for (auto& bb: function.basicBlocks()) {
        for (auto instItr = bb.instructions.begin(); instItr != bb.instructions.end(); ) {
            auto* const load = dyncast<Load*>(instItr.to_address());
            if (load && load->userCount() == 0) {
                instItr = bb.instructions.erase(instItr);
                continue;
            }
            ++instItr;
            if (!load) {
                continue;
            }
            auto* const phi = promoteLoad(load);
            if (!phi) {
                continue;
            }
            if (phi->argumentCount() == 1) {
                auto* value = phi->argumentAt(0).value;
                auto* load = std::prev(instItr).to_address();
                for (auto* user: load->users()) {
                    for (auto [index, op]: utl::enumerate(user->operands())) {
                        if (op == load) {
                            user->setOperand(index, value);
                        }
                    }
                }
                bb.instructions.erase(std::prev(instItr));
                continue;
            }
            bb.instructions.erase(std::prev(instItr));
            bb.instructions.insert(instItr, phi);
        }
    }
}

Phi* Ctx::promoteLoad(Load* load) {
    auto* const addr = load->address();
    utl::small_vector<Store*> storesToAddr;
    /// Gather all the stores to this address.
    for (auto* use: addr->users()) {
        if (auto* store = dyncast<Store*>(use)) { storesToAddr.push_back(store); }
    }
    /// \Warning If we have no stores or no stores for every predecessor we have to figure something else out.
    auto relevantStores = findRelevantStores(storesToAddr, load);
    utl::small_vector<PhiMapping> phiArgs;
    for (auto [pred, rp]: relevantStores) {
        auto [store, dist] = rp;
        phiArgs.push_back({ pred, store->source() });
    }
    return new Phi(phiArgs, std::string(load->name()));
}

utl::hashmap<BasicBlock*, Ctx::RelevantStoreResultPair> Ctx::findRelevantStores(std::span<Store* const> stores, Load* load) {
    utl::hashmap<BasicBlock*, RelevantStoreResultPair> relevantStores;
    auto* addr = load->address();
    for (auto* store: stores) {
        auto paths = findAllPaths(store->parent(), load->parent());
        SC_ASSERT(!paths.empty(), "No paths found. This can't be right, or can it?");
        for (auto& path: paths) {
            auto* lastStore = findLastStoreToAddress(path, addr);
            if (lastStore != store) {
                continue;
            }
            RelevantStoreResultPair const rp = { store, path.size() };
            auto const [itr, success] = relevantStores.insert({ *(path.end() - 2), rp });
            if (success) {
                continue;
            }
            SC_ASSERT(rp.distance != itr->second.distance, "");
            if (itr->second.distance > rp.distance) {
                itr->second = rp;
            }
        }
    }
    return relevantStores;
}

Store* Ctx::findLastStoreToAddress(std::span<BasicBlock* const> path, Value const* address) {
    for (auto* bb: utl::reverse(path)) {
        for (auto& inst: utl::reverse(bb->instructions)) {
            auto* store = dyncast<Store*>(&inst);
            if (!store) {
                continue;
            }
            if (store->dest() == address) {
                return store;
            }
        }
    }
    return nullptr;
}

/// ** Stores **

void Ctx::evictDeadStores(Function& function) {
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

bool Ctx::isDead(Store const* store) {
    Value const* const address = store->dest();
    Function const& currentFunction = *store->parent()->parent();
    if (!isLocalToFunction(address, currentFunction)) {
        /// We can only guarantee that this store is dead if the memory was locally allocated by this function.
        return false;
    }
    /// Gather all the loads to this address.
    for (BasicBlock const& bb: currentFunction.basicBlocks()) {
        for (Instruction const& inst: bb.instructions) {
            Load const* load = dyncast<Load const*>(&inst);
            if (!load) { continue; }
            bool const loadIsReachable = isReachable(store, load);
            if (loadIsReachable) {
                return false;                
            }
        }
    }
    return true;
}

bool Ctx::isLocalToFunction(Value const* address, Function const& function) {
    for (BasicBlock const& bb: function.basicBlocks()) {
        for (Instruction const& inst: bb.instructions) {
            auto* allocaInst = dyncast<Alloca const*>(&inst);
            if (!allocaInst) { continue; }
            if (allocaInst == address) {
                return true;
            }
        }
    }
    return false;
}

/// ** Generic methods **

namespace {

struct PathFinder {
    using Map = std::map<size_t, utl::small_vector<BasicBlock*>>;
    using Iterator = Map::iterator;
    
    void run() {
        auto [itr, success] = result.insert({ id++, { origin } });
        SC_ASSERT(success, "Why not?");
        search(itr);
    }
    
    void search(Iterator currentPath) {
        auto currentNode = currentPath->second.back();
        if (currentNode == dest) {
            return;
        }
        switch (currentNode->successors.size()) {
        case 0:
            result.erase(currentPath);
            break;
        case 1:
            currentPath->second.push_back(currentNode->successors.front());
            search(currentPath);
            break;
        default:
            currentPath->second.push_back(currentNode->successors.front());
            auto cpCopy = currentPath->second;
            search(currentPath);
            for (size_t i = 1; i < currentNode->successors.size(); ++i) {
                auto [itr, success] = result.insert({ id++, cpCopy });
                SC_ASSERT(success, "");
                itr->second.back() = currentNode->successors[i];
                search(itr);
            }
            break;
        }
    }
    
    BasicBlock* origin;
    BasicBlock* dest;
    size_t id = 0;
    Map result{};
};

} // namespace

utl::vector<utl::small_vector<BasicBlock*>> Ctx::findAllPaths(BasicBlock* origin, BasicBlock* dest) {
    PathFinder pf{ origin, dest };
    pf.run();
    return utl::vector<utl::small_vector<BasicBlock*>>(
               utl::transform(std::move(pf.result),
                              [](auto&& pair) -> decltype(auto) { return UTL_FORWARD(pair).second; }));
}

bool Ctx::isReachable(Instruction const* from, Instruction const* to) {
    SC_ASSERT(from != to, "from and to are equal. Does that mean they are reachable or not?");
    if (from->parent() == to->parent()) {
        /// From and to are in the same basic block. If \p to comes after \p from then they are definitely reachable.
        auto* bb = from->parent();
        for (Instruction const* i = from; i != bb->instructions.end().to_address(); i = i->next()) {
            if (i == to) { return true; }
        }
    }
    /// If they are not in the same basic block or \p to comes before \p from perform a DFS to check if we can reach the BB of \p to from the BB of \p from.
    auto search = [&, target = to->parent()](BasicBlock const* bb, auto& search) -> bool {
        if (bb == target) {
            return true;
        }
        for (BasicBlock const* succ: bb->successors) {
            if (search(succ, search)) {
                return true;
            }
        }
        return false;
    };
    return search(from->parent(), search);
}
