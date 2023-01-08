#include "Opt/Mem2Reg.h"

#include <map>

#include "IR/CFG.h"
#include "IR/Module.h"

using namespace scatha;
using namespace ir;

namespace {

struct Ctx {
    Ctx(Module& mod): mod(mod) {}
    
    void run();
    
    void doFunction(Function& function);
    
    void analyzeLoad(Load* load);
    
    utl::vector<utl::small_vector<BasicBlock*>> findAllPaths(BasicBlock* origin, BasicBlock* dest);
    
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
            analyzeLoad(load);
        }
    }
}

#include <iostream>
#include "IR/Print.h"

void Ctx::analyzeLoad(Load* load) {
    auto* const addr = load->address();
    utl::small_vector<Store*> storesToAddr;
    /// Gather all the stores to this address.
    for (auto* use: addr->users()) {
        if (auto* store = dyncast<Store*>(use)) { storesToAddr.push_back(store); }
    }
    std::cout << "Stores to address of %" << load->name() << ":\n";
    for (auto* store: storesToAddr) {
        auto paths = findAllPaths(store->parent(), load->parent());
        for (auto& path: paths) {
            std::cout << "\tOne path is: ";
            for (bool first = true; auto* bb: path) {
                std::cout << (first ? (void)(first = false), "" : " -> ") << bb->name();
            }
            std::cout << std::endl;
        }
        if (paths.empty()) {
            std::cout << "Warning: No paths found\n";
        }
        auto* source = store->source();
        std::cout << "\t%" << "" << " : " << toString(*source) << std::endl;
    }
    if (storesToAddr.empty()) { std::cout << "\tWarning: No stores\n"; }
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
    return utl::vector<utl::small_vector<BasicBlock*>>(utl::transform(std::move(pf.result), [](auto&& pair) -> decltype(auto) { return UTL_FORWARD(pair).second; }));
}
