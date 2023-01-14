#include "Opt/Mem2Reg.h"

#include <algorithm>

#include <utl/hashset.hpp>
#include <utl/vector.hpp>

#include "Basic/Basic.h"
#include "IR/Context.h"
#include "IR/CFG.h"
#include "IR/Module.h"
#include "IR/Validate.h"
#include "Opt/Common.h"

using namespace scatha;
using namespace scatha::opt;
using namespace scatha::ir;

namespace {

struct Mem2RegContext {
    explicit Mem2RegContext(ir::Context& context, Function& function): irCtx(context), function(function) {}
    
    void run();
    
    bool promote(Load* load);
  
    Value* search(BasicBlock*, size_t depth, size_t bifurkations);
    
    Value* findReplacement(Value* value);
    
    void evictIfDead(Store* store);
    
    bool isDead(Store const* store);
    
    void evictIfDead(Alloca* inst);
    
    bool isDead(Alloca const* inst);
    
    void gather();
    
    ir::Context& irCtx;
    Function& function;
    
    /// Maps pairs of basic blocks and addresses to lists of load and store instructions from and to that address in that basic block.
    utl::hashmap<std::pair<BasicBlock*, Value*>, utl::small_vector<Instruction*>> loadsAndStores;
    /// Maps evicted load instructions to their respective replacement values.
    utl::hashmap<Load const*, Value*> loadReplacementMap;
    /// List of all load instructions in the function.
    utl::small_vector<Load*> loads;
    /// List of all store instructions in the function.
    utl::small_vector<Store*> stores;
    /// List of all alloca instructions in the function.
    utl::hashset<Alloca*> allocas;
    
    void setCurrentLoad(Load* load) {
        _currentLoad = load;
    }
    
    Load* currentLoad() { return _currentLoad; }
    
    Load* _currentLoad = nullptr;
};

} // namespace

void opt::mem2Reg(ir::Context& irCtx, ir::Module& mod) {
    for (auto& function: mod.functions()) {
        Mem2RegContext ctx(irCtx, function);
        ctx.run();
    }
    ir::assertInvariants(irCtx, mod);
}

void Mem2RegContext::run() {
    gather();
    for (auto const load: loads) {
        promote(load);
    }
    for (auto* const store: stores) {
        evictIfDead(store);
    }
    for (auto* const inst: allocas) {
        evictIfDead(inst);
    }
}

bool Mem2RegContext::promote(Load* load) {
    setCurrentLoad(load);
    auto* const basicBlock = currentLoad()->parent();
    Value* newValue = search(basicBlock, 0, 0);
    if (!newValue) {
        return false;
    }
    loadReplacementMap[currentLoad()] = newValue;
    currentLoad()->setName("evicted-load");
    basicBlock->instructions.erase(currentLoad());
    replaceValue(currentLoad(), newValue);
    currentLoad()->clearOperands();
    setCurrentLoad({});
    return true;
}

static Phi* findPhiWithArgs(BasicBlock* basicBlock, std::span<PhiMapping const> args) {
    Phi* phi = nullptr;
    for (auto itr = basicBlock->instructions.begin();
         itr != basicBlock->instructions.end() && (phi = dyncast<Phi*>(itr.to_address())) != nullptr;
         ++itr)
    {
        if (compareEqual(phi, args)) {
            return phi;
        }
    }
    return nullptr;
}

Value* Mem2RegContext::search(BasicBlock* basicBlock, size_t depth, size_t bifurkations) {
    auto& ls = loadsAndStores[{ basicBlock, currentLoad()->address() }];
    auto ourLoad = [&]{ return std::find(ls.begin(), ls.end(), currentLoad()); };
    auto const beginItr = basicBlock != currentLoad()->parent() || depth == 0 ? ls.begin() : ourLoad();
    auto const endItr = depth > 0 ? ls.end() : ourLoad();
    if (beginItr != endItr) {
        auto const itr = endItr - 1;
        /// This basic block has a load or store that we use to promote
        auto* const result = visit(**itr, utl::overload{
            [](Load& load) { return &load; },
            [](Store& store) { return store.source(); },
            [](auto&) -> Value* { SC_UNREACHABLE(); }
        });
        return findReplacement(result);
    }
    SC_ASSERT(depth == 0 || basicBlock != currentLoad()->parent(), "If we are back in our starting BB we must have found ourself as a matching load.");
    /// This basic block has no load or store that we use to promote
    /// We need to visit our predecessors
    switch (basicBlock->predecessors.size()) {
    case 0:
        return nullptr;
    case 1:
        return search(basicBlock->predecessors.front(), depth + 1, bifurkations);
    default:
        utl::small_vector<PhiMapping> phiArgs;
        size_t const predCount = basicBlock->predecessors.size();
        phiArgs.reserve(predCount);
        size_t numPredsEqualToSelf = 0;
        Value* valueUnequalToSelf = nullptr;
        for (auto* pred: basicBlock->predecessors) {
            PhiMapping arg{ pred, search(pred, depth + 1, bifurkations + 1) };
            SC_ASSERT(arg.value, "This probably just means we can't promote or have to insert a load into pred, but we figure it out later.");
            phiArgs.push_back(arg);
            if (arg.value == currentLoad()) {
                ++numPredsEqualToSelf;
            }
            else {
                valueUnequalToSelf = arg.value;
            }
        }
        SC_ASSERT(numPredsEqualToSelf < predCount, "How can all predecessors refer to this load? How would we than even reach this basic block?");
        if (numPredsEqualToSelf == predCount - 1) {
            return valueUnequalToSelf;
        }
        if (auto* phi = findPhiWithArgs(basicBlock, phiArgs)) {
            return phi;
        }
        std::string name = bifurkations == 0 ? std::string(currentLoad()->name()) : irCtx.uniqueName(&function, currentLoad()->name(), ".p", bifurkations);
        auto* phi = new Phi(std::move(phiArgs), std::move(name));
        basicBlock->addInstruction(basicBlock->instructions.begin(), phi);
        return phi;
    }
}

Value* Mem2RegContext::findReplacement(Value* value) {
    decltype(loadReplacementMap)::iterator itr;
    while ((itr = loadReplacementMap.find(value)) != loadReplacementMap.end()) {
        value = itr->second;
    }
    return value;
}

void Mem2RegContext::evictIfDead(Store* store) {
    if (!isDead(store)) {
        return;
    }
    store->parent()->instructions.erase(store);
    store->clearOperands();
}

bool Mem2RegContext::isDead(Store const* store) {
    Value const* const address = store->dest();
    if (!allocas.contains(address)) {
        /// We can only guarantee that this store is dead if the memory was locally allocated by this function.
        return false;
    }
    for (auto* const load: loads) {
        if (load->address() != address) { continue; }
        bool const loadIsReachable = isReachable(store, load);
        if (loadIsReachable) {
            return false;
        }
    }
    return true;
}

void Mem2RegContext::evictIfDead(Alloca* inst) {
    if (!isDead(inst)) {
        return;
    }
    inst->parent()->instructions.erase(inst);
}

bool Mem2RegContext::isDead(Alloca const* address) {
    /// See if there is any load from this address.
    /// We can query the cached loads like this, because the evicted loads have their operands set to null, so they will compare unequal here.
    for (auto* const load: loads) {
        if (load->address() != address) { continue; }
        return false;
    }
    return true;
}

void Mem2RegContext::gather() {
    for (auto& inst: function.instructions()) {
        visit(inst, utl::overload{ // clang-format off
            [&](Load& load) {
                loads.push_back(&load);
                loadsAndStores[{ load.parent(), load.address() }].push_back(&load);
            },
            [&](Store& store) {
                stores.push_back(&store);
                loadsAndStores[{ store.parent(), store.dest() }].push_back(&store);
            },
            [&](Alloca& inst) {
                allocas.insert(&inst);
            },
            [&](auto&) {},
        }); // clang-format on
    }
    /// Assertion code
    for (auto&& [bb, ls]: loadsAndStores) {
        SC_ASSERT(std::is_sorted(ls.begin(), ls.end(), [](Instruction const* a, Instruction const* b){ return preceeds(a, b); }), "Loads and stores in one basic block must be sorted by position");
    }
}
