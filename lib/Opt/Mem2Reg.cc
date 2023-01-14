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

template <typename I = Instruction>
struct InstructionContext {
    I* instruction;
    size_t positionInBB;
};

static constexpr auto lsCmpLs = [](auto const& a, auto const& b) { return a.positionInBB < b.positionInBB; };

struct Mem2RegContext {
    explicit Mem2RegContext(ir::Context& context, Function& function): irCtx(context), function(function) {}
    
    void run();
    
    bool promote(InstructionContext<Load>);
  
    Value* search(BasicBlock*, size_t depth, size_t bifurkations);
    
    Value* findReplacement(Value* value);
    
    void evictIfDead(Store*);
    
    bool isDead(Store const*);
    
    void evictIfDead(Alloca*);
    
    bool isDead(Alloca const*);
    
    void gather();
    
    ir::Context& irCtx;
    Function& function;
    
    /// Maps pairs of basic blocks and addresses to lists of load and store instructions from and to that address in that basic block.
    utl::hashmap<std::pair<BasicBlock*, Value*>, utl::vector<InstructionContext<Instruction>>> loadsAndStores;
    /// Maps evicted load instructions to their respective replacement values.
    utl::hashmap<Load const*, Value*> loadReplacementMap;
    /// List of all load instructions in the function.
    utl::vector<InstructionContext<Load>> loads;
    /// List of all store instructions in the function.
    utl::vector<Store*> stores;
    /// List of all alloca instructions in the function.
    utl::hashset<Alloca*> allocas;
    
    void setCurrentLoad(InstructionContext<Load> c) {
        _currentLoad = c.instruction;
        _currentLoadPositionInBB = c.positionInBB;
    }
    
    Load* currentLoad() { return _currentLoad; }
    size_t currentLoadPositionInBB() { return _currentLoadPositionInBB; }
    
    Load* _currentLoad = nullptr;
    size_t _currentLoadPositionInBB = 0;
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

bool Mem2RegContext::promote(InstructionContext<Load> loadContext) {
    setCurrentLoad(loadContext);
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
    auto ourLoad = [&]{
        return std::find_if(ls.begin(), ls.end(), [&](InstructionContext<> const& c) { return c.positionInBB == currentLoadPositionInBB(); });
    };
    auto const beginItr = basicBlock != currentLoad()->parent() || depth == 0 ? ls.begin() : ourLoad();
    auto const endItr = depth > 0 ? ls.end() : ourLoad();
    auto const itr = std::max_element(beginItr, endItr, lsCmpLs);
    if (itr != endItr) {
        /// This basic block has a load or store that we use to promote
        auto* const result = visit(*itr->instruction, utl::overload{
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
    for (auto const [load, _]: loads) {
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
    /// See if there is any load from this address
    for (auto const [load, _]: loads) {
        if (load->address() != address) { continue; }
        return false;
    }
    return true;
}

void Mem2RegContext::gather() {
    for (auto& bb: function.basicBlocks()) {
        for (auto&& [index, inst]: utl::enumerate(bb.instructions)) {
            visit(inst, utl::overload{ // clang-format off
                [&, index = index](Load& load) {
                    loads.push_back({ &load, index });
                    loadsAndStores[{ load.parent(), load.address() }].push_back({ &load, index });
                },
                [&, index = index](Store& store) {
                    stores.push_back(&store);
                    loadsAndStores[{ store.parent(), store.dest() }].push_back({ &store, index });
                },
                [&](Alloca& inst) {
                    allocas.insert(&inst);
                },
                [&](auto&) {},
            }); // clang-format on
        }
    }
    /// Assertion code
    for (auto&& [bb, ls]: loadsAndStores) {
        SC_ASSERT(std::is_sorted(ls.begin(), ls.end(), lsCmpLs), "Loads and stores in one basic block must be sorted");
    }
}
