#include "Opt/Mem2Reg.h"

#include <algorithm>
#include <optional>

#include <boost/logic/tribool.hpp>
#include <utl/hashtable.hpp>
#include <utl/scope_guard.hpp>
#include <utl/vector.hpp>

#include "Basic/Basic.h"
#include "Common/Expected.h"
#include "Common/UniquePtr.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Validate.h"
#include "Opt/Common.h"

using namespace scatha;
using namespace scatha::opt;
using namespace scatha::ir;

namespace {

enum class SearchException { NoResult, Cycle };

struct Mem2RegContext {
    explicit Mem2RegContext(ir::Context& context, Function& function): irCtx(context), function(function) {}

    bool run();

    bool promote(Load* load);

    Expected<Value*, SearchException> search(BasicBlock*, size_t depth, size_t bifurkations);

    Expected<Value*, SearchException> combinePredecessors(BasicBlock*, size_t depth, size_t bifurkations);

    Value* findReplacement(Value* value);

    void evict(Instruction* inst);

    bool isDead(Store const* store);

    static bool isUnused(Instruction const* inst);

    void gather();

    void setCurrentLoad(Load* load) { _currentLoad = load; }

    Load* currentLoad() { return _currentLoad; }

    Value* currentAddress() { return currentLoad()->address(); }

    ir::Context& irCtx;
    Function& function;

    /// LoadAndStoreMap
    using LoadAndStoreKey = std::pair<BasicBlock*, Value*>;
    struct LoadAndStoreKeyHash {
        size_t operator()(LoadAndStoreKey const& key) const { return std::hash<BasicBlock*>{}(key.first); }
    };
    struct LoadAndStoreKeyEqual {
        bool operator()(LoadAndStoreKey const& a, LoadAndStoreKey const& b) const {
            return a.first == b.first && addressEqual(a.second, b.second);
        }
    };
    using LoadAndStoreMap =
        utl::hashmap<LoadAndStoreKey, utl::small_vector<Instruction*>, LoadAndStoreKeyHash, LoadAndStoreKeyEqual>;

    /// Maps pairs of basic blocks and addresses to lists of load and store instructions from and to that address in
    /// that basic block.
    LoadAndStoreMap loadsAndStores;
    /// Maps evicted load instructions to their respective replacement values.
    utl::hashmap<Load const*, Value*> loadReplacementMap;
    /// List of all load instructions in the function.
    utl::small_vector<Load*> loads;
    /// Evicted loads will be destroyed with the context object.
    utl::small_vector<UniquePtr<Load>> evictedLoads;
    /// List of all store instructions in the function.
    utl::small_vector<Store*> stores;
    /// List of all other memory instructions in the function. For now that is \p alloca 's and \p gep 's
    utl::small_vector<Instruction*> otherMemInstructions;

    /// Set of basic blocks visited by \p search() pass.
    utl::hashset<BasicBlock const*> visitedBBs;

    Load* _currentLoad = nullptr;

    /// Map basic blocks and the address of the current load to phi nodes that correspond to the value in that memory
    /// location.
    Phi* correspondingPhi(BasicBlock* basicBlock) {
        auto itr = _loadPhiMap.find({ basicBlock, currentAddress() });
        return itr == _loadPhiMap.end() ? nullptr : itr->second;
    }

    void setCorrespondingPhi(BasicBlock* basicBlock, Phi* phi) { _loadPhiMap[{ basicBlock, currentAddress() }] = phi; }

    void removeCorrespondingPhi(BasicBlock* basicBlock) { _loadPhiMap.erase({ basicBlock, currentAddress() }); }

    using LoadPhiMap = utl::hashmap<LoadAndStoreKey, Phi*, LoadAndStoreKeyHash, LoadAndStoreKeyEqual>;
    LoadPhiMap _loadPhiMap;
};

} // namespace

bool opt::mem2Reg(ir::Context& irCtx, ir::Function& function) {
    Mem2RegContext ctx(irCtx, function);
    bool const result = ctx.run();
    ir::assertInvariants(irCtx, function);
    return result;
}

bool Mem2RegContext::run() {
    gather();
    bool modifiedAny = false;
    for (auto const load: loads) {
        modifiedAny |= promote(load);
    }
    for (auto* const store: stores) {
        if (isDead(store)) {
            evict(store);
            modifiedAny = true;
        }
    }
    for (auto* const inst: otherMemInstructions) {
        if (isUnused(inst)) {
            evict(inst);
            modifiedAny = true;
        }
    }
    return modifiedAny;
}

bool Mem2RegContext::promote(Load* load) {
    setCurrentLoad(load);
    auto* const basicBlock = currentLoad()->parent();
    auto searchResult      = search(basicBlock, 0, 0);
    if (!searchResult) {
        return false;
    }
    Value* const newValue             = *searchResult;
    loadReplacementMap[currentLoad()] = newValue;
    currentLoad()->setName("evicted-load");
    replaceValue(currentLoad(), newValue);
    currentLoad()->clearOperands();
    /// We extract here because the loads need to stay alive until the algorithm is finished.
    evictedLoads.push_back(UniquePtr<Load>(cast<Load*>(basicBlock->extract(currentLoad()))));
    setCurrentLoad(nullptr);
    return true;
}

Expected<Value*, SearchException> Mem2RegContext::search(BasicBlock* basicBlock, size_t depth, size_t bifurkations) {
    auto& ls            = loadsAndStores[{ basicBlock, currentLoad()->address() }];
    auto ourLoad        = [&] { return std::find(ls.begin(), ls.end(), currentLoad()); };
    auto const beginItr = basicBlock != currentLoad()->parent() || depth == 0 ? ls.begin() : ourLoad();
    auto const endItr   = depth > 0 ? ls.end() : ourLoad();
    /// We search loads, stores and phi nodes in this basic block in reverse order. Phi nodes always appear first so
    /// we can search them separately after searching loads and stores.
    /// Search the loads and stores in this basic block:
    if (beginItr != endItr) {
        /// This basic block has a load or store that we use to promote
        auto const itr = endItr - 1;
        // clang-format off
        auto* const result = visit(**itr, utl::overload{
            [](Load& load) { return &load; },
            [](Store& store) { return store.source(); },
            [](auto&) -> Value* { SC_UNREACHABLE(); }
        }); // clang-format on
        return findReplacement(result);
    }
    /// Search the phi nodes in this basic block corresponding to the target load:
    if (Value* result = correspondingPhi(basicBlock)) {
        if (bifurkations == 0) {
            result->setName(std::string(currentLoad()->name()));
        }
        return result;
    }
    SC_ASSERT(depth == 0 || basicBlock != currentLoad()->parent(),
              "If we are back in our starting BB we must have found ourself as a matching load.");
    if (visitedBBs.contains(basicBlock)) {
        return SearchException::Cycle;
    }
    visitedBBs.insert({ basicBlock, nullptr });
    utl::scope_guard eraseVisited = [&] { visitedBBs.erase(basicBlock); };
    /// This basic block has no load or store that we can use to promote. We need to visit our predecessors.
    switch (basicBlock->predecessors().size()) {
    case 0: return SearchException::NoResult;
    case 1: return search(basicBlock->predecessors().front(), depth + 1, bifurkations);
    default: return combinePredecessors(basicBlock, depth, bifurkations);
    }
}

/// We could actually delete this function as it is only used for a trivial assertion.
static Phi* findPhiWithArgs(BasicBlock* basicBlock, std::span<PhiMapping const> args) {
    for (auto& inst: *basicBlock) {
        Phi* const phi = dyncast<Phi*>(&inst);
        if (phi == nullptr) {
            break;
        }
        if (compareEqual(phi, args)) {
            return phi;
        }
    }
    return nullptr;
}

Expected<Value*, SearchException> Mem2RegContext::combinePredecessors(BasicBlock* basicBlock,
                                                                      size_t depth,
                                                                      size_t bifurkations) {
    utl::small_vector<PhiMapping, 6> phiArgs;
    size_t const predCount = basicBlock->predecessors().size();
    phiArgs.reserve(predCount);
    size_t numPredsEqualToSelf = 0;
    Value* valueUnequalToSelf  = nullptr;
    auto* phi                  = new Phi(currentLoad()->type());
    SC_ASSERT(correspondingPhi(basicBlock) == nullptr, "This should be handled by search()");
    setCorrespondingPhi(basicBlock, phi);
    utl::armed_scope_guard deletePhi = [&] {
        delete phi;
        removeCorrespondingPhi(basicBlock);
    };
    for (auto* pred: basicBlock->predecessors()) {
        auto const searchResult = search(pred, depth + 1, bifurkations + 1);
        if (!searchResult && searchResult.error() != SearchException::Cycle) {
            /// Here may be opportunity for further optimization, as we potentially could load conditionally.
            return searchResult.error();
        }
        PhiMapping arg{ pred, searchResult.valueOr(phi) };
        SC_ASSERT(arg.value, "We never return nullptrs form search()");
        phiArgs.push_back(arg);
        if (arg.value == currentLoad()) {
            ++numPredsEqualToSelf;
        }
        else {
            valueUnequalToSelf = arg.value;
        }
    }
    SC_ASSERT(numPredsEqualToSelf < predCount,
              "How can all predecessors refer to this load? How would we than even reach this basic block?");
    SC_ASSERT(phiArgs.size() == predCount || phiArgs.size() == 1, "?");
    if (numPredsEqualToSelf == predCount - 1) {
        return valueUnequalToSelf;
    }
    if (phiArgs.size() == 1) {
        return phiArgs.front().value;
    }
    if (auto* phi = findPhiWithArgs(basicBlock, phiArgs)) {
        // TODO: Properly implement this case.
        SC_DEBUGFAIL(); // This might actually not be an error. If we already have phi nodes in the function before
                        // running mem2reg pass, this case could happen.
        return phi;
    }
    deletePhi.disarm();
    std::string name = bifurkations == 0 ? std::string(currentLoad()->name()) :
                                           irCtx.uniqueName(&function, currentLoad()->name(), ".p", bifurkations);
    phi->setName(std::move(name));
    phi->setArguments(std::move(phiArgs));
    basicBlock->insert(basicBlock->begin(), phi);
    return phi;
}

Value* Mem2RegContext::findReplacement(Value* value) {
    using Iter = decltype(loadReplacementMap)::iterator;
    utl::small_vector<Iter, 16> iters;
    while (true) {
        auto iter = loadReplacementMap.find(value);
        if (iter == loadReplacementMap.end()) {
            break;
        }
        iters.push_back(iter);
        value = iter->second;
    }
    for (auto iter: iters) {
        iter->second = value;
    }
    return value;
}

void Mem2RegContext::evict(Instruction* inst) {
    inst->parent()->erase(inst);
}

static std::optional<std::tuple<Value const*, size_t, size_t>> getConstantBaseAndOffset(Value const* addr) {
    GetElementPointer const* gep = nullptr;
    Value const* base            = addr;
    size_t offset                = 0;
    size_t size                  = cast<PointerType const*>(addr->type())->pointeeType()->size();
    while ((gep = dyncast<GetElementPointer const*>(base)) != nullptr) {
        if (!gep->isAllConstant()) {
            return std::nullopt;
        }
        base = gep->basePointer();
        offset += gep->constantByteOffset();
        size = gep->pointeeType()->size();
    }
    return std::tuple{ base, offset, size };
}

static bool testOverlap(size_t aBegin, size_t aSize, size_t bBegin, size_t bSize) {
    return aBegin <= bBegin + bSize && bBegin <= aBegin + aSize;
}

static boost::tribool testAddressOverlap(Value const* a, Value const* b) {
    if (!a || !b) {
        return false;
    }
    if (a == b) {
        return true;
    }
    auto abo = getConstantBaseAndOffset(a);
    auto bbo = getConstantBaseAndOffset(b);
    if (!abo || !bbo) {
        return boost::indeterminate;
    }
    auto const [aBase, aOffset, aSize] = *abo;
    auto const [bBase, bOffset, bSize] = *bbo;
    return aBase == bBase && testOverlap(aOffset, aSize, bOffset, bSize);
}

bool Mem2RegContext::isDead(Store const* store) {
    Value const* const destAddress = store->dest();
    if (!isLocalMemory(destAddress)) {
        /// We can only guarantee that this store is dead if the memory was locally allocated by this function.
        return false;
    }
    for (auto* const load: loads) {
        if (!testAddressOverlap(load->address(), destAddress)) {
            continue;
        }
        bool const loadIsReachable = isReachable(store, load);
        if (loadIsReachable) {
            return false;
        }
    }
    return true;
}

bool Mem2RegContext::isUnused(Instruction const* inst) {
    return inst->users().empty();
}

void Mem2RegContext::gather() {
    for (auto& inst: function.instructions()) {
        // clang-format off
        visit(inst, utl::overload{
            [&](Load& load) {
                loads.push_back(&load);
                loadsAndStores[{ load.parent(), load.address() }].push_back(&load);
            },
            [&](Store& store) {
                stores.push_back(&store);
                loadsAndStores[{ store.parent(), store.dest() }].push_back(&store);
            },
            [&](Alloca& inst) {
                otherMemInstructions.push_back(&inst);
            },
            [&](GetElementPointer& gep) {
                otherMemInstructions.push_back(&gep);
            },
            [&](auto&) {},
        }); // clang-format on
    }
    for ([[maybe_unused]] auto&& [bb_addr, ls]: loadsAndStores) {
        SC_ASSERT(std::is_sorted(ls.begin(),
                                 ls.end(),
                                 [](Instruction const* a, Instruction const* b) { return preceeds(a, b); }),
                  "Cached loads and stores in one basic block must be sorted by position");
    }
}
