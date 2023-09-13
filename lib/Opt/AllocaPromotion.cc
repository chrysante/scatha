#include "Opt/AllocaPromotion.h"

#include <string>

#include <range/v3/algorithm.hpp>
#include <utl/hashtable.hpp>
#include <utl/stack.hpp>
#include <utl/vector.hpp>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Dominance.h"
#include "IR/Type.h"

using namespace scatha;
using namespace ir;
using namespace opt;

bool opt::isPromotable(Alloca const* allocaInst) {
    size_t size = allocaInst->allocatedType()->size();
    return ranges::all_of(allocaInst->users(), [&](Instruction const* inst) {
        // clang-format off
        return SC_MATCH (*inst) {
        [&] (Load const& load) {
            return load.type()->size() == size;
        },
        [&] (Store const& store) {
            return store.value() != allocaInst &&
                   store.value()->type()->size() == size;
        },
        [&] (Instruction const&) {
            return false;
        }
        }; // clang-format on
    });
}

namespace {

struct VariableInfo {
    Alloca* address = nullptr;
    Type const* type = nullptr;
    std::string name;
    Context& ctx;
    DominanceInfo const& domInfo;
    utl::small_vector<Load*> loads;
    utl::hashset<BasicBlock*> usingBlocks;
    utl::small_vector<Store*> stores;
    utl::hashset<BasicBlock*> definingBlocks;
    utl::hashmap<BasicBlock*, Phi*> BBToPhiMap;
    utl::hashset<Phi*> phiNodes;
    utl::hashset<BasicBlock const*> renamedBlocks;
    utl::stack<uint32_t> stack;
    utl::small_vector<Value*> versions;
    uint32_t counter = 0;

    void setVersion(size_t index, Value* value) {
        if (versions.size() < index + 1) {
            versions.resize(index + 1);
        }
        versions[index] = value;
    }

    Phi* getPhi(BasicBlock* BB) const {
        auto phiItr = BBToPhiMap.find(BB);
        if (phiItr != BBToPhiMap.end()) {
            return phiItr->second;
        }
        return nullptr;
    }

    VariableInfo(ir::Alloca& inst, Context& ctx, DominanceInfo const& domInfo);

    utl::hashset<BasicBlock*> computeLiveBlocks();

    void insertPhis();

    void genName(Value* value);

    void rename(BasicBlock* BB);

    void clean();
};

} // namespace

void opt::promoteAlloca(Alloca* inst,
                        Context& ctx,
                        DominanceInfo const& domInfo) {
    auto* function = inst->parentFunction();
    VariableInfo info(*inst, ctx, domInfo);
    info.insertPhis();
    info.rename(&function->entry());
    info.clean();
}

VariableInfo::VariableInfo(Alloca& allocaInst,
                           Context& ctx,
                           DominanceInfo const& domInfo):
    address(&allocaInst),
    type(allocaInst.allocatedType()),
    name(allocaInst.name()),
    ctx(ctx),
    domInfo(domInfo) {
    for (auto* inst: address->users()) {
        // clang-format off
        SC_MATCH (*inst) {
            [&](Store& store) {
                SC_ASSERT(store.address() == address, "Not promotable");
                stores.push_back(&store);
                definingBlocks.insert(store.parent());
            },
            [&](Load& load) {
                SC_ASSERT(load.address() == address, "Not promotable");
                loads.push_back(&load);
                usingBlocks.insert(load.parent());
            },
            [&](Instruction const& inst) { SC_UNREACHABLE("Not promotable"); }
        }; // clang-format on
    }
}

/// Stolen from here:
/// https://github.com/llvm-mirror/llvm/blob/master/lib/Transforms/Utils/PromoteMemoryToRegister.cpp#L773
utl::hashset<BasicBlock*> VariableInfo::computeLiveBlocks() {
    utl::hashset<BasicBlock*> result;
    auto worklist = usingBlocks | ToSmallVector<>;
    for (auto itr = worklist.begin(); itr != worklist.end(); ++itr) {
        auto* bb = *itr;
        if (!definingBlocks.contains(bb)) {
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
            if (definingBlocks.contains(pred)) {
                continue;
            }

            /// Otherwise it is, add to the worklist.
            worklist.push_back(pred);
        }
    }

    return result;
}

void VariableInfo::insertPhis() {
    auto const liveBlocks = computeLiveBlocks();
    auto appearedOnWorklist = definingBlocks;
    auto worklist = appearedOnWorklist | ToSmallVector<>;
    while (!worklist.empty()) {
        BasicBlock* x = worklist.back();
        worklist.pop_back();
        auto dfX = domInfo.domFront(x);
        for (auto* y: dfX) {
            if (BBToPhiMap.contains(y)) {
                continue;
            }
            if (!liveBlocks.contains(y)) {
                continue;
            }
            auto* undefVal = ctx.undef(type);
            auto phiArgs = y->predecessors() |
                           ranges::views::transform([&](BasicBlock* pred) {
                               return PhiMapping(pred, undefVal);
                           }) |
                           ToSmallVector<>;
            /// Name will be set later in `genName()`
            auto* phi = new Phi(std::move(phiArgs), std::string{});
            y->pushFront(phi);
            BBToPhiMap[y] = phi;
            phiNodes.insert(phi);
            if (!appearedOnWorklist.contains(y)) {
                appearedOnWorklist.insert(y);
                worklist.push_back(y);
            }
        }
    }
}

void VariableInfo::genName(Value* value) {
    uint32_t const i = counter;
    setVersion(i, value);
    stack.push(i);
    counter = i + 1;
    if (auto* phi = dyncast<Phi*>(value)) {
        phi->setName(name);
    }
}

void VariableInfo::rename(BasicBlock* BB) {
    if (renamedBlocks.contains(BB)) {
        return;
    }
    renamedBlocks.insert(BB);
    if (auto* phi = getPhi(BB)) {
        genName(phi);
    }
    for (auto& inst: *BB) {
        // clang-format off
        SC_MATCH (inst) {
            [&](Load& load) {
                auto* allocaInst = dyncast<Alloca*>(load.address());
                if (allocaInst != address) {
                    return;
                }
                Value* value = nullptr;
                if (!stack.empty()) {
                    size_t i = stack.top();
                    value = versions[i];
                }
                else {
                    /// The stack being empty means we load from
                    /// uninitialized memory, so we replace the load
                    /// with `undef`
                    value = ctx.undef(load.type());
                }
                load.replaceAllUsesWith(value);
            },
            [&](Store& store) {
                auto* allocaInst = dyncast<Alloca*>(store.address());
                if (allocaInst == address) {
                    genName(store.value());
                }
            },
            [&](Instruction&) {}
        }; // clang-format on
    }
    for (auto* succ: BB->successors()) {
        if (stack.empty()) {
            continue;
        }
        if (auto* phi = getPhi(succ)) {
            phi->setArgument(BB, versions[stack.top()]);
        }
    }
    for (auto* succ: BB->successors()) {
        rename(succ);
    }
    for (auto& inst: *BB) {
        auto* ourPhi = getPhi(BB);
        // clang-format off
        // TODO: Rename to isDef??
        bool isPartOfThis = SC_MATCH (inst) {
            [&](Phi& phi) {
                return &phi == ourPhi;
            },
            [&](Store& store) {
                return address == dyncast<Alloca*>(store.address());
            },
            [&](Instruction const& inst) { return false; }
        }; // clang-format on
        if (isPartOfThis) {
            stack.pop();
        }
    }
}

void VariableInfo::clean() {
    for (auto* load: loads) {
        SC_ASSERT(load->users().empty(), "Should be empty after promotion");
        load->parent()->erase(load);
    }
    for (auto* store: stores) {
        store->parent()->erase(store);
    }
    for (auto [BB, phi]: BBToPhiMap) {
        if (phi->users().empty()) {
            phi->parent()->erase(phi);
        }
    }
    SC_ASSERT(address->users().empty(), "Should be empty after promotion");
    address->parent()->erase(address);
}
