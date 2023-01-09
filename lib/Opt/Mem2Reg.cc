#include "Opt/Mem2Reg.h"

#include <map>

#include <utl/hashmap.hpp>
#include <utl/vector.hpp>

#include "IR/CFG.h"
#include "IR/Module.h"


#include <iostream>
#include "IR/Print.h"


using namespace scatha;
using namespace ir;

namespace {

struct Path {
    utl::small_vector<BasicBlock const*> bbs;
    Instruction const* begin;
    Instruction const* end; // Note: This is _not_ past the end.
};

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
    
    utl::hashmap<BasicBlock const*, RelevantStoreResultPair> findRelevantStores(std::span<Store* const> stores, Load* load);
    
    Store const* findLastStoreToAddress(Path const& path, Value const* address);
    
    /// ** Stores **
    
    void evictDeadStores(Function& function);
    
    /// Analyze a \p Store instruction and return true if this is a store to local variable that is not being read anymore.
    bool isDead(Store const* store);
    
    /// Check wether this function contains an \p alloca instruction that allocated the memory of \p address
    bool isLocalToFunction(Value const* address, Function const& function);
   
    /// ** Allocas **
    
    void evictDeadAllocas(Function& function);

    /// Analyze a \p Alloca instruction and return true it's memory is not being read.
    bool isDead(Alloca const* address);
    
    /// ** Generic methods **
    
    utl::vector<Path> findAllPaths(Instruction const* origin, Instruction const* dest);
    
    /// Analyze wether there is a control flow path from \p from to \p to
    bool isReachable(Instruction const* from, Instruction const* to);
    
    /// Analyze wether \p a comes before \p b
    /// \pre \p a and \p b must be in the same bsaic block.
    bool preceeds(Instruction const* a, Instruction const* b);
    
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
//        evictDeadStores(function);
//        evictDeadAllocas(function);
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
        phiArgs.push_back({ /*fu*/ const_cast<BasicBlock*>(pred), store->source() });
    }
    return new Phi(phiArgs, std::string(load->name()));
}

static void debugPrint(BasicBlock const* bb) {
    std::cout << "In basic block: " << bb->name() << ":\n";
}

static void debugPrint(Load const* load) {
    debugPrint(load->parent());
    std::cout << " Analyzing load: " << *load << ":\n";
}

static void debugPrint(Store const* store) {
    std::cout << "  "; debugPrint(store->parent());
    std::cout << "   Analyzing store: " << *store << ":\n";
}

static void printPath(Path const& path) {
    bool first = true;
    for (auto* bb: path.bbs) {
        std::cout << (first ? (void)(first = false), "" : " -> ") << bb->name();
    }
}

static void debugPrint(Path const& path) {
    std::cout << "    Path: "; printPath(path); std::cout << std::endl;
}

utl::hashmap<BasicBlock const*, Ctx::RelevantStoreResultPair> Ctx::findRelevantStores(std::span<Store* const> stores, Load* load) {
    utl::hashmap<BasicBlock const*, RelevantStoreResultPair> relevantStores;
    auto* addr = load->address();
    debugPrint(load);
    for (auto* store: stores) {
        debugPrint(store);
        utl::vector<Path> const paths = findAllPaths(store, load);
        SC_ASSERT(!paths.empty(), "No paths found. This can't be right, or can it?");
        for (Path const& path: paths) {
            debugPrint(path);
            SC_ASSERT(!path.bbs.empty(), "How can the path be empty?");
            if (path.bbs.size() == 1 && preceeds(load, store)) {
                std::cout << "We ignore this store as the load preceeds it in the same BB\n";
                continue;
            }
            Store const* lastStore = findLastStoreToAddress(path, addr);
            if (lastStore != store) {
                std::cout << "     We ignore this store; ";
                if (lastStore) {
                    std::cout << "Last store is: " << *lastStore;
                }
                std::cout << std::endl;
                continue;
            }
            RelevantStoreResultPair const rp = { store, path.bbs.size() };
            auto const [itr, success] = relevantStores.insert({ *(path.bbs.end() - 2), rp });
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

Store const* Ctx::findLastStoreToAddress(Path const& path, Value const* address) {
    for (auto&& bb: utl::reverse(path.bbs)) {
        bool const firstVisitedBB = &bb == &path.bbs.back();
        bool const lastVisitedBB = &bb == &path.bbs.front();
        auto* beginInst = firstVisitedBB ? path.end   : &bb->instructions.back();
        auto* backInst  = lastVisitedBB  ? path.begin : &bb->instructions.front();
        auto* endInst   = backInst->prev();
        
        for (auto* i = beginInst; i != endInst; i = i->prev()) {
            auto* store = dyncast<Store const*>(i);
            if (!store) {
                continue;
            }
            if (store->dest() == address) {
                return store;
            }
        }
        
//        for (auto& inst: utl::reverse(bb->instructions)) {
//            auto* store = dyncast<Store*>(&inst);
//            if (!store) {
//                continue;
//            }
//            if (store->dest() == address) {
//                return store;
//            }
//        }
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
            /// \Warning What if the loaded address is a GEP instruction that is based on \p address ???
            if (load->address() != address) { continue; }
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

/// ** Allocas **

void Ctx::evictDeadAllocas(Function& function) {
    for (auto& bb: function.basicBlocks()) {
        for (auto instItr = bb.instructions.begin(); instItr != bb.instructions.end(); ) {
            auto* const address = dyncast<Alloca*>(instItr.to_address());
            if (!address) { ++instItr; continue; }
            bool const canErase = isDead(address);
            if (canErase) {
                instItr = bb.instructions.erase(instItr);
                continue;
            }
            ++instItr;
        }
    }
}

bool Ctx::isDead(Alloca const* address) {
    Function const& currentFunction = *address->parent()->parent();
    /// See if there is any load from this address
    for (BasicBlock const& bb: currentFunction.basicBlocks()) {
        for (Instruction const& inst: bb.instructions) {
            Load const* load = dyncast<Load const*>(&inst);
            if (!load) { continue; }
            /// \Warning What about GEPs, see \p Store case.
            if (load->address() != address) { continue; }
            return false;
        }
    }
    return true;
}

/// ** Generic methods **

namespace {

struct PathFinder {
    using Map = std::map<size_t, Path>;
    using Iterator = Map::iterator;
    
    explicit PathFinder(Instruction const* origin, Instruction const* dest):
        origin(origin),
        dest(dest),
        originBB(origin->parent()),
        destBB(dest->parent()) {}
    
    
    
    void run() {
        auto [itr, success] = result.insert({ id++, Path{ .bbs = {}, .begin = origin, .end = dest } });
        SC_ASSERT(success, "Why not?");
        search(itr, originBB);
    }
    
    static bool contains(std::span<BasicBlock const* const> path, BasicBlock const* bb) {
        return std::find(path.begin(), path.end(), bb) != path.end();
    }
    
    void search(Iterator currentPathItr, BasicBlock const* currentNode) {
        auto& currentPath = currentPathItr->second;
        if (contains(currentPath.bbs, currentNode) && currentPath.bbs.front() != currentNode) {
            return;
        }
        currentPath.bbs.push_back(currentNode);
        if (currentNode == destBB && currentPath.bbs.size() > 1) {
            return;
        }
        switch (currentNode->successors.size()) {
        case 0:
            result.erase(currentPathItr);
            break;
        case 1:
            search(currentPathItr, currentNode->successors.front());
            break;
        default:
            auto cpCopy = currentPath;
            search(currentPathItr, currentNode->successors.front());
            for (size_t i = 1; i < currentNode->successors.size(); ++i) {
                auto [itr, success] = result.insert({ id++, cpCopy });
                SC_ASSERT(success, "");
                search(itr, currentNode->successors[i]);
            }
            break;
        }
    }
    
    Instruction const* origin;
    Instruction const* dest;
    BasicBlock const* originBB;
    BasicBlock const* destBB;
    size_t id = 0;
    Map result{};
};

} // namespace

utl::vector<Path> Ctx::findAllPaths(Instruction const* origin, Instruction const* dest) {
    PathFinder pf{ origin, dest };
    pf.run();
    return utl::vector<Path>(utl::transform(std::move(pf.result),
                                            [](auto&& pair) -> decltype(auto) { return UTL_FORWARD(pair).second; }));
}

bool Ctx::isReachable(Instruction const* from, Instruction const* to) {
    SC_ASSERT(from != to, "from and to are equal. Does that mean they are reachable or not?");
    if (from->parent() == to->parent()) {
        /// From and to are in the same basic block. If \p from preceeds \p to then \p to is definitely reachable.
        if (preceeds(from, to)) {
            return true;
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

bool Ctx::preceeds(Instruction const* a, Instruction const* b) {
    SC_ASSERT(a->parent() == b->parent(), "a and b must be in the same basic block");
    auto* bb = a->parent();
    auto const* const end = bb->instructions.end().to_address();
    for (; a != end; a = a->next()) {
        if (a == b) { return true; }
    }
    return false;
}
