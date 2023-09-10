#include "Opt/Passes.h"

#include <string>

#include <utl/hashtable.hpp>
#include <utl/stack.hpp>
#include <utl/vector.hpp>

#include "Common/Ranges.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Dominance.h"
#include "IR/Validate.h"
#include "Opt/Common.h"
#include "Opt/PassRegistry.h"

using namespace scatha;
using namespace ir;
using namespace opt;

SC_REGISTER_PASS(opt::memToReg, "memtoreg");

namespace {

struct VariableInfo {
    void setVersion(size_t index, Value* value) {
        if (versions.size() < index + 1) {
            versions.resize(index + 1);
        }
        versions[index] = value;
    }

    Type const* type = nullptr;
    std::string name;
    utl::small_vector<Load*> loads;
    utl::hashset<BasicBlock*> usingBlocks;
    utl::small_vector<Store*> stores;
    utl::hashset<BasicBlock*> definingBlocks;
    utl::hashmap<BasicBlock*, Phi*> phiNodes;
    utl::stack<uint32_t> stack;
    utl::small_vector<Value*> versions;
    uint32_t counter = 0;
};

struct MemToRegContext {
    MemToRegContext(Context& irCtx, Function& function):
        irCtx(irCtx),
        function(function),
        domInfo(function.getOrComputeDomInfo()) {}

    bool run();

    VariableInfo gatherInfo(Alloca* address);

    utl::hashset<BasicBlock*> computeLiveBlocks(Alloca* address);

    void insertPhis(Alloca* address, VariableInfo& info);

    void renameVariables(BasicBlock* basicBlock);

    void genName(Alloca* addr, Value* value);

    bool clean();

    Context& irCtx;
    Function& function;
    DominanceInfo const& domInfo;
    utl::hashmap<Alloca*, VariableInfo> variables;
    utl::hashset<BasicBlock const*> renamedBlocks;
    /// Map phis to corresponding allocas
    utl::hashmap<Phi*, Alloca*> phiMap;
};

} // namespace

static bool isPromotable(Alloca const& allc) {
    for (auto* user: allc.users()) {
        if (isa<Load>(user)) {
            continue;
        }
        if (auto* store = dyncast<Store const*>(user);
            store && store->address() == &allc)
        {
            continue;
        }
        return false;
    }
    return true;
}

bool opt::memToReg(Context& irCtx, Function& function) {
    /// We run the algorithm repeatedly until the function is not modified
    /// anymore. Multiple passes may be needed if addresses of stack allocations
    /// are stored in other stack allocations. Per loop iteration one level of
    /// stack memory indirection is peeled off.
    /// TODO: Make this smarter
    int const HardLimit = 10;
    bool result = false;
    for (int i = 0; i < HardLimit; ++i) {
        MemToRegContext ctx(irCtx, function);
        bool intermediateResult = ctx.run();
        if (!intermediateResult) {
            break;
        }
        assertInvariants(irCtx, function);
        result = true;
    }
    return result;
}

bool MemToRegContext::run() {
    variables =
        function.entry() | ranges::views::filter([](Instruction& inst) {
            return isa<Alloca>(inst) && isPromotable(cast<Alloca const&>(inst));
        }) |
        ranges::views::transform([this](Instruction& inst) {
            auto* addr = dyncast<Alloca*>(&inst);
            return std::pair{ addr, gatherInfo(addr) };
        }) |
        ranges::to<utl::hashmap<Alloca*, VariableInfo>>;
    for (auto&& [addr, info]: variables) {
        insertPhis(addr, info);
    }
    renameVariables(&function.entry());
    return clean();
}

VariableInfo MemToRegContext::gatherInfo(Alloca* address) {
    VariableInfo result;
    result.type = address->allocatedType();
    result.name = std::string(address->name());
    for (auto* user: address->users()) {
        // clang-format off
        visit(*user, utl::overload{
            [&](Store& store) {
                if (store.address() == address) {
                    result.stores.push_back(&store);
                    result.definingBlocks.insert(store.parent());
                }
            },
            [&](Load& load) {
                SC_ASSERT(load.address() == address, "");
                result.loads.push_back(&load);
                result.usingBlocks.insert(load.parent());
            },
            [&](Instruction const& inst) { SC_UNREACHABLE(); }
        }); // clang-format on
    }
    return result;
}

/// Stolen from here:
/// https://github.com/llvm-mirror/llvm/blob/master/lib/Transforms/Utils/PromoteMemoryToRegister.cpp#L773
utl::hashset<BasicBlock*> MemToRegContext::computeLiveBlocks(Alloca* address) {
    utl::hashset<BasicBlock*> result;
    auto& info = variables.find(address)->second;
    auto worklist = info.usingBlocks | ToSmallVector<>;
    for (auto itr = worklist.begin(); itr != worklist.end(); ++itr) {
        auto* bb = *itr;
        if (!info.definingBlocks.contains(bb)) {
            continue;
        }
        /// Okay, this is a block that both uses and defines the value. If the
        /// first reference to the alloca is a def (store), then we know it
        /// isn't live-in.
        for (auto i = bb->begin();; ++i) {
            if (auto* store = dyncast<Store const*>(i.to_address())) {
                if (store->address() != address) {
                    continue;
                }
                /// We found a store to the alloca before a load. The alloca is
                /// not actually live-in here.
                *itr = worklist.back();
                worklist.pop_back();
                --itr;
                break;
            }
            if (Load* load = dyncast<Load*>(i.to_address())) {
                /// Okay, we found a load before a store to the alloca. It is
                /// actually live into this block.
                if (load->address() == address) {
                    break;
                }
            }
        }
    }
    /// Now that we have a set of blocks where the phi is live-in, recursively
    /// add their predecessors until we find the full region the value is live.
    while (!worklist.empty()) {
        BasicBlock* bb = worklist.back();
        worklist.pop_back();

        /// The block really is live in here, insert it into the set. If already
        /// in the set, then it has already been processed.
        if (!result.insert(bb).second) {
            continue;
        }

        /// Since the value is live into BB, it is either defined in a
        /// predecessor or live into it too. Add the preds to the worklist
        /// unless they are a defining block.
        for (auto* pred: bb->predecessors()) {
            /// The value is not live into a predecessor if it defines the
            /// value.
            if (info.definingBlocks.contains(pred)) {
                continue;
            }

            /// Otherwise it is, add to the worklist.
            worklist.push_back(pred);
        }
    }

    return result;
}

void MemToRegContext::insertPhis(Alloca* address, VariableInfo& varInfo) {
    SC_ASSERT(isPromotable(*address), "");
    auto const liveBlocks = computeLiveBlocks(address);
    auto appearedOnWorklist = varInfo.definingBlocks;
    auto worklist = appearedOnWorklist | ToSmallVector<>;
    while (!worklist.empty()) {
        BasicBlock* x = worklist.back();
        worklist.pop_back();
        auto dfX = domInfo.domFront(x);
        for (auto* y: dfX) {
            if (varInfo.phiNodes.contains(y)) {
                continue;
            }
            if (!liveBlocks.contains(y)) {
                continue;
            }
            auto* undefVal = irCtx.undef(varInfo.type);
            auto phiArgs = y->predecessors() |
                           ranges::views::transform([&](BasicBlock* pred) {
                               return PhiMapping(pred, undefVal);
                           }) |
                           ToSmallVector<>;
            /// Name will be set later in `genName()`
            auto* phi = new Phi(std::move(phiArgs), std::string{});
            phiMap.insert({ phi, address });
            y->pushFront(phi);
            varInfo.phiNodes[y] = phi;
            if (!appearedOnWorklist.contains(y)) {
                appearedOnWorklist.insert(y);
                worklist.push_back(y);
            }
        }
    }
}

void MemToRegContext::genName(Alloca* addr, Value* value) {
    SC_ASSERT(variables.contains(addr), "");
    auto& info = variables.find(addr)->second;
    uint32_t const i = info.counter;
    info.setVersion(i, value);
    info.stack.push(i);
    info.counter = i + 1;
    if (auto* phi = dyncast<Phi*>(value)) {
        phi->setName(info.name);
    }
}

void MemToRegContext::renameVariables(BasicBlock* basicBlock) {
    if (renamedBlocks.contains(basicBlock)) {
        return;
    }
    renamedBlocks.insert(basicBlock);
    for (auto& phi: basicBlock->phiNodes()) {
        auto itr = phiMap.find(&phi);
        if (itr == phiMap.end()) {
            continue;
        }
        genName(itr->second, &phi);
    }
    for (auto& inst: *basicBlock) {
        // clang-format off
        SC_MATCH (inst) {
            [&](Load& load) {
                auto* address = dyncast<Alloca*>(load.address());
                if (!variables.contains(address)) {
                    return;
                }
                auto& info = variables.find(address)->second;
                Value* value = nullptr;
                if (!info.stack.empty()) {
                    size_t i = info.stack.top();
                    value = info.versions[i];
                }
                else {
                    /// The stack being empty means we load from
                    /// uninitialized memory, so we replace the load
                    /// with `undef`
                    value = irCtx.undef(load.type());
                }
                load.replaceAllUsesWith(value);
            },
            [&](Store& store) {
                auto* address = dyncast<Alloca*>(store.address());
                if (!variables.contains(address)) {
                    return;
                }
                genName(address, store.value());
            },
            [&](Instruction&) {}
        }; // clang-format on
    }
    for (auto* succ: basicBlock->successors()) {
        for (auto& phi: succ->phiNodes()) {
            auto const itr = phiMap.find(&phi);
            if (itr == phiMap.end()) {
                continue;
            }
            auto* address = itr->second;
            SC_ASSERT(variables.contains(address), "");
            auto& info = variables.find(address)->second;
            if (info.stack.empty()) {
                continue;
            }
            size_t const i = info.stack.top();
            phi.setArgument(basicBlock, info.versions[i]);
        }
    }
    for (auto* succ: basicBlock->successors()) {
        renameVariables(succ);
    }
    for (auto& inst: *basicBlock) {
        // clang-format off
        auto* address = visit(inst, utl::overload{
            [&](Phi& phi) {
                auto itr = phiMap.find(&phi);
                return itr != phiMap.end() ? itr->second : nullptr;
            },
            [&](Store& store) -> Alloca* {
                auto* addr = dyncast<Alloca*>(store.address());
                if (!addr) {
                    return nullptr;
                }
                if (!variables.contains(addr)) {
                    return nullptr;
                }
                return addr;
            },
            [&](Instruction const& inst) { return nullptr; }
        }); // clang-format on
        if (!address) {
            continue;
        }
        auto& info = variables.find(address)->second;
        info.stack.pop();
    }
}

bool MemToRegContext::clean() {
    bool cleanedAny = false;
    for (auto& [address, info]: variables) {
        for (auto* load: info.loads) {
            SC_ASSERT(load->users().empty(), "Should be empty after promotion");
            load->parent()->erase(load);
        }
        for (auto* store: info.stores) {
            store->parent()->erase(store);
        }
        SC_ASSERT(address->users().empty(), "Should be empty after promotion");
        address->parent()->erase(address);
        cleanedAny = true;
    }
    for (auto [phi, address]: phiMap) {
        if (phi->users().empty()) {
            phi->parent()->erase(phi);
            cleanedAny = true;
        }
    }
    return cleanedAny;
}
