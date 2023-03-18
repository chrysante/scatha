#include "Opt/Mem2Reg.h"

#include <algorithm>
#include <optional>

#include <boost/logic/tribool.hpp>
#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>
#include <utl/scope_guard.hpp>
#include <utl/stack.hpp>
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

enum class SearchError { NoResult, Cycle };

struct SearchContext {
    explicit SearchContext(Load* currentLoad,
                           Value* currentAddress,
                           std::string name):
        _currentLoad(currentLoad),
        _currentAddress(currentAddress),
        _name(std::move(name)) {}

    Load* load() { return _currentLoad; }

    Value* address() { return _currentAddress; }

    std::string_view name() const { return _name; }

    /// Set of basic blocks visited by `search()` pass.
    utl::hashset<BasicBlock const*> visitedBBs;
    Load* _currentLoad;
    Value* _currentAddress;
    std::string _name;
};

struct Mem2RegContext {
    explicit Mem2RegContext(ir::Context& context, Function& function):
        irCtx(context), function(function) {}

    bool run();

    bool promote(Load* load);

    Expected<Value*, SearchError> search(BasicBlock* start, Load* load);
    Expected<Value*, SearchError> search(BasicBlock* start,
                                         Load* load,
                                         Value* address,
                                         std::string name);

    Expected<Value*, SearchError> searchImpl(BasicBlock*,
                                             size_t depth,
                                             size_t bifurkations);

    Expected<Value*, SearchError> combinePredecessors(BasicBlock*,
                                                      size_t depth,
                                                      size_t bifurkations);

    Value* findReplacement(Value* value);

    void evict(Instruction* inst);

    bool isDead(Store const* store);

    static bool isUnused(Instruction const* inst);

    void gather();

    ir::Context& irCtx;
    Function& function;

    /// LoadAndStoreMap
    using LoadAndStoreKey = std::pair<BasicBlock*, Value*>;
    struct LoadAndStoreKeyHash {
        size_t operator()(LoadAndStoreKey const& key) const {
            return std::hash<BasicBlock*>{}(key.first);
        }
    };
    struct LoadAndStoreKeyEqual {
        bool operator()(LoadAndStoreKey const& a,
                        LoadAndStoreKey const& b) const {
            return a.first == b.first && addressEqual(a.second, b.second);
        }
    };
    using LoadAndStoreMap = utl::hashmap<LoadAndStoreKey,
                                         utl::small_vector<Instruction*>,
                                         LoadAndStoreKeyHash,
                                         LoadAndStoreKeyEqual>;

    /// Maps pairs of basic blocks and addresses to lists of load and store
    /// instructions from and to that address in that basic block.
    LoadAndStoreMap loadsAndStores;
    /// Maps evicted load instructions to their respective replacement values.
    utl::hashmap<Load const*, Value*> loadReplacementMap;
    /// List of all load instructions in the function.
    utl::small_vector<Load*> loads;
    /// Evicted loads will be destroyed with the context object.
    utl::small_vector<UniquePtr<Load>> evictedLoads;
    /// List of all store instructions in the function.
    utl::small_vector<Store*> stores;

    /// Map basic blocks and the address of the current load to phi nodes that
    /// correspond to the value in that memory location.
    Phi* correspondingPhi(BasicBlock* basicBlock, Value* address) {
        auto itr = _loadPhiMap.find({ basicBlock, address });
        return itr == _loadPhiMap.end() ? nullptr : itr->second;
    }

    void setCorrespondingPhi(BasicBlock* basicBlock, Value* address, Phi* phi) {
        _loadPhiMap[{ basicBlock, address }] = phi;
    }

    void removeCorrespondingPhi(BasicBlock* basicBlock, Value* address) {
        _loadPhiMap.erase({ basicBlock, address });
    }

    using LoadPhiMap = utl::hashmap<LoadAndStoreKey,
                                    Phi*,
                                    LoadAndStoreKeyHash,
                                    LoadAndStoreKeyEqual>;
    LoadPhiMap _loadPhiMap;

    std::optional<SearchContext> searchContext;
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
    return modifiedAny;
}

bool Mem2RegContext::promote(Load* load) {
    auto* const basicBlock = load->parent();
    auto searchResult      = search(basicBlock, load);
    if (!searchResult) {
        return false;
    }
    Value* const newValue    = *searchResult;
    loadReplacementMap[load] = newValue;
    load->setName("evicted-load");
    replaceValue(load, newValue);
    load->clearOperands();
    /// We extract here because the loads need to stay alive until the algorithm
    /// is finished.
    evictedLoads.push_back(
        UniquePtr<Load>(cast<Load*>(basicBlock->extract(load))));
    return true;
}

Expected<Value*, SearchError> Mem2RegContext::search(BasicBlock* start,
                                                     Load* load) {
    return search(start, load, load->address(), std::string(load->name()));
}

Expected<Value*, SearchError> Mem2RegContext::search(BasicBlock* start,
                                                     Load* load,
                                                     Value* address,
                                                     std::string name) {
    searchContext = SearchContext(load, address, std::move(name));
    utl::scope_guard resetSearcgContext = [&] { searchContext = std::nullopt; };
    return searchImpl(start, 0, 0);
}

/// While promoting a load:
///     If we encounter a store to a region of memory
///     - `=` equal to our load:
///              We call that our result.
///     - `⊂` enclosing our load:
///              We insert a sequence of extract-element instructions at the
///              store and call that our result.
///     - `⊃` enclosed by our load:
///              We insert a sequence of insert-element instructions at the
///              store and continue the search.
///     - `?` whose relation to our load is indeterminable:
///              We insert a load there.
///

Expected<Value*, SearchError> Mem2RegContext::searchImpl(BasicBlock* basicBlock,
                                                         size_t depth,
                                                         size_t bifurkations) {
    auto& ls     = loadsAndStores[{ basicBlock, searchContext->address() }];
    auto ourLoad = [&] {
        return std::find(ls.begin(), ls.end(), searchContext->load());
    };
    auto const beginItr =
        basicBlock != searchContext->load()->parent() || depth == 0 ?
            ls.begin() :
            ourLoad();
    auto const endItr = depth > 0 ? ls.end() : ourLoad();
    /// We search loads, stores and phi nodes in this basic block in reverse
    /// order. Phi nodes always appear first so we can search them separately
    /// after searching loads and stores. Search the loads and stores in this
    /// basic block:

    /// Search the entire basic block here, not just the cached loads and stores
    /// and then remove the caches entirely from this file.
    //    std::find_if(basicBlock->begin());...

    auto reverseBB = ranges::views::reverse(*basicBlock);
    for (auto i = reverseBB.begin(), end = reverseBB.end(); i != end; ++i) {
    }

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
    /// Search the phi nodes in this basic block corresponding to the target
    /// load:
    if (Value* result = correspondingPhi(basicBlock, searchContext->address()))
    {
        if (bifurkations == 0) {
            result->setName(std::string(searchContext->name()));
        }
        return result;
    }
    SC_ASSERT(depth == 0 || basicBlock != searchContext->load()->parent(),
              "If we are back in our starting BB we must have found ourself as "
              "a matching load.");
    if (searchContext->visitedBBs.contains(basicBlock)) {
        return SearchError::Cycle;
    }
    searchContext->visitedBBs.insert(basicBlock);
    utl::scope_guard eraseVisited = [&] {
        searchContext->visitedBBs.erase(basicBlock);
    };
    /// This basic block has no load or store that we can use to promote. We
    /// need to visit our predecessors.
    switch (basicBlock->predecessors().size()) {
    case 0: return SearchError::NoResult;
    case 1:
        return searchImpl(basicBlock->predecessors().front(),
                          depth + 1,
                          bifurkations);
    default: return combinePredecessors(basicBlock, depth, bifurkations);
    }
}

/// We could actually delete this function as it is only used for a trivial
/// assertion.
static Phi* findPhiWithArgs(BasicBlock* basicBlock,
                            std::span<PhiMapping const> args) {
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

Expected<Value*, SearchError> Mem2RegContext::combinePredecessors(
    BasicBlock* basicBlock, size_t depth, size_t bifurkations) {
    utl::small_vector<PhiMapping, 6> phiArgs;
    size_t const predCount = basicBlock->predecessors().size();
    phiArgs.reserve(predCount);
    size_t numPredsEqualToSelf = 0;
    Value* valueUnequalToSelf  = nullptr;
    auto* phi                  = new Phi(searchContext->load()->type());
    SC_ASSERT(correspondingPhi(basicBlock, searchContext->address()) == nullptr,
              "This should be handled by search()");
    setCorrespondingPhi(basicBlock, searchContext->address(), phi);
    utl::armed_scope_guard deletePhi = [&] {
        delete phi;
        removeCorrespondingPhi(basicBlock, searchContext->address());
    };
    for (auto* pred: basicBlock->predecessors()) {
        auto const searchResult = searchImpl(pred, depth + 1, bifurkations + 1);
        if (!searchResult && searchResult.error() != SearchError::Cycle) {
            /// Here may be opportunity for further optimization, as we
            /// potentially could load conditionally.
            return searchResult.error();
        }
        PhiMapping arg{ pred, searchResult.valueOr(phi) };
        SC_ASSERT(arg.value, "We never return nullptrs form search()");
        phiArgs.push_back(arg);
        if (arg.value == searchContext->load()) {
            ++numPredsEqualToSelf;
        }
        else {
            valueUnequalToSelf = arg.value;
        }
    }
    if (numPredsEqualToSelf == predCount) {
        return searchContext->load();
    }
    SC_ASSERT(phiArgs.size() == predCount || phiArgs.size() == 1, "?");
    if (numPredsEqualToSelf == predCount - 1) {
        return valueUnequalToSelf;
    }
    if (phiArgs.size() == 1) {
        return phiArgs.front().value;
    }
    if (auto* phi = findPhiWithArgs(basicBlock, phiArgs)) {
        // TODO: Properly implement this case.
        SC_DEBUGFAIL(); // This might actually not be an error. If we already
                        // have phi nodes in the function before running mem2reg
                        // pass, this case could happen.
        return phi;
    }
    deletePhi.disarm();
    std::string name = bifurkations == 0 ?
                           std::string(searchContext->name()) :
                           irCtx.uniqueName(&function,
                                            searchContext->name(),
                                            ".p",
                                            bifurkations);
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
            [&](Instruction const&) {},
        }); // clang-format on
    }
    for ([[maybe_unused]] auto&& [bb_addr, ls]: loadsAndStores) {
        SC_ASSERT(std::is_sorted(ls.begin(), ls.end(), &opt::preceeds),
                  "Cached loads and stores in one basic block must be sorted "
                  "by position");
    }
}
