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
    
    void doFunction(Function& function);
    
    /// Analyze a \p Load instruction and return a \p Phi instruction that can replace it.
    Phi* analyzeLoad(Load* load);
    
    struct RelevantStoreResultPair {
        Store* store;
        size_t distance;
    };
    
    utl::hashmap<BasicBlock*, RelevantStoreResultPair> findRelevantStores(std::span<Store* const> stores, Load* load);
    
    utl::vector<utl::small_vector<BasicBlock*>> findAllPaths(BasicBlock* origin, BasicBlock* dest);
    
    Store* findLastStoreToAddress(std::span<BasicBlock* const> path, Value const* address);
    
    Module& mod;
};

} // namespace

void opt::mem2Reg(ir::Module& mod) {
    Ctx ctx(mod);
    ctx.run();
}

void Ctx::run() {
    for (auto& function: mod.functions()) {
        doFunction(function);
    }
}

void Ctx::doFunction(Function& function) {
    for (auto& bb: function.basicBlocks()) {
        for (auto instItr = bb.instructions.begin(); instItr != bb.instructions.end(); ) {
            auto* const load = dyncast<Load*>(instItr.to_address());
            if (load && load->users().empty()) {
                instItr = bb.instructions.erase(instItr);
                continue;
            }
            ++instItr;
            if (!load) {
                continue;
            }
            auto* const phi = analyzeLoad(load);
            if (phi) {
                bb.instructions.erase(std::prev(instItr));
                bb.instructions.insert(instItr, phi);
            }
        }
    }
}

Phi* Ctx::analyzeLoad(Load* load) {
    auto* const addr = load->address();
    utl::small_vector<Store*> storesToAddr;
    /// Gather all the stores to this address.
    for (auto* use: addr->users()) {
        if (auto* store = dyncast<Store*>(use)) { storesToAddr.push_back(store); }
    }
    // Warning: If we have no stores or no stores for every predecessor we have to figure something else out.
    auto relevantStores = findRelevantStores(storesToAddr, load);
    utl::small_vector<PhiMapping> phiArgs;
    for (auto [pred, rp]: relevantStores) {
        auto [store, dist] = rp;
        phiArgs.push_back({ .pred = pred, .value = store->source() });
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
