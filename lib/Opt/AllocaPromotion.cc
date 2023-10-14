#include "Opt/AllocaPromotion.h"

#include <string>

#include <range/v3/algorithm.hpp>
#include <utl/functional.hpp>
#include <utl/hashtable.hpp>
#include <utl/stack.hpp>
#include <utl/vector.hpp>

#include "IR/Builder.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Dominance.h"
#include "IR/Type.h"
#include "Opt/Common.h"

using namespace scatha;
using namespace ir;
using namespace opt;

bool opt::isPromotable(Alloca const* allocaInst) {
    auto* constantCount = dyncast<IntegralConstant const*>(allocaInst->count());
    if (!constantCount) {
        return false;
    }
    size_t size = constantCount->value().to<size_t>() *
                  allocaInst->allocatedType()->size();
    return ranges::all_of(allocaInst->users(), [&](Instruction const* inst) {
        // clang-format off
        return SC_MATCH (*inst) {
        [&](Load const& load) {
            return load.type()->size() == size;
        },
        [&](Store const& store) {
            return store.value() != allocaInst &&
                   store.value()->type()->size() == size;
        },
        [&](Call const& call) {
            if (isConstSizeMemcpy(&call)) {
                return memcpySize(&call) == size &&
                       memcpyDest(&call) != memcpySource(&call);
            }
            /// Technically we could allow any constant memset call but I can't be bothered to implement this right now.
            if (isConstZeroMemset(&call)) {
                return memsetSize(&call) == size;
            }
            return false;
        },
        [&](Instruction const&) {
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
    utl::small_vector<Instruction*> uses;
    utl::hashset<BasicBlock*> usingBlocks;
    utl::small_vector<Instruction*> defs;
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

    /// Computes all blocks where this alloca is live, that is all blocks where
    /// we need to insert phi instructions
    utl::hashset<BasicBlock*> computeLiveBlocks();

    /// Insert phi instructions where necessary
    void insertPhis();

    void genName(Value* value);

    Value* getLastDef() {
        if (stack.empty()) {
            return nullptr;
        }
        size_t i = stack.top();
        return versions[i];
    }

    /// \Returns \p value, possibly bitcast to \p type if \p value has a type
    /// different than \p type
    Value* bitcast(Value* value, Instruction* insertPoint, Type const* type);

    bool renameDef(Instruction* inst);

    bool renameUse(Instruction* inst);

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

bool opt::tryPromoteAlloca(Alloca* inst,
                           Context& ctx,
                           DominanceInfo const& domInfo) {
    if (isPromotable(inst)) {
        promoteAlloca(inst, ctx, domInfo);
        return true;
    }
    return false;
}

static Type const* allocatedArrayType(Context& ctx, Alloca const* inst) {
    auto* type = inst->allocatedType();
    size_t size = inst->constantCount().value();
    if (size == 1) {
        return type;
    }
    return ctx.arrayType(type, size);
}

VariableInfo::VariableInfo(Alloca& allocaInst,
                           Context& ctx,
                           DominanceInfo const& domInfo):
    address(&allocaInst),
    type(allocatedArrayType(ctx, &allocaInst)),
    name(allocaInst.name()),
    ctx(ctx),
    domInfo(domInfo) {
    for (auto* inst: address->users()) {
        // clang-format off
        SC_MATCH (*inst) {
            [&](Store& store) {
                SC_ASSERT(store.address() == address, "Not promotable");
                defs.push_back(&store);
                definingBlocks.insert(store.parent());
            },
            [&](Load& load) {
                SC_ASSERT(load.address() == address, "Not promotable");
                uses.push_back(&load);
                usingBlocks.insert(load.parent());
            },
            [&](Call& call) {
                if (isMemcpy(&call)) {
                    auto* dest = memcpyDest(&call);
                    auto* source = memcpySource(&call);
                    SC_ASSERT(dest == address || source == address, "Not promotable");
                    if (dest == address) {
                        defs.push_back(&call);
                        definingBlocks.insert(call.parent());
                    }
                    else {
                        uses.push_back(&call);
                        usingBlocks.insert(call.parent());
                    }
                }
                else if (isMemset(&call)) {
                    auto* dest = memsetDest(&call);
                    SC_ASSERT(dest == address, "Not promotable");
                    defs.push_back(&call);
                    definingBlocks.insert(call.parent());
                }
                else {
                    SC_UNREACHABLE();
                }
            },
            [&](Instruction const& inst) { SC_UNREACHABLE("Not promotable"); }
        }; // clang-format on
    }
}

static Value const* definingAddress(Instruction const* inst) {
    if (auto* store = dyncast<Store const*>(inst)) {
        return store->address();
    }
    if (isMemcpy(inst)) {
        return memcpyDest(inst);
    }
    if (isMemset(inst)) {
        return memsetDest(inst);
    }
    return nullptr;
}

static Value* definingAddress(Instruction* inst) {
    return const_cast<Value*>(definingAddress(&std::as_const(*inst)));
}

static Value const* usingAddress(Instruction const* inst) {
    if (auto* load = dyncast<Load const*>(inst)) {
        return load->address();
    }
    if (isMemcpy(inst)) {
        return memcpySource(inst);
    }
    return nullptr;
}

static Value* usingAddress(Instruction* inst) {
    return const_cast<Value*>(usingAddress(&std::as_const(*inst)));
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
            if (definingAddress(i.to_address()) == address) {
                /// We found a store to the alloca before a load. The alloca is
                /// not actually live-in here.
                *itr = worklist.back();
                worklist.pop_back();
                --itr;
                break;
            }
            if (usingAddress(i.to_address()) == address) {
                /// Okay, we found a load before a store to the alloca. It is
                /// actually live into this block.
                break;
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

Value* VariableInfo::bitcast(Value* value,
                             Instruction* insertPoint,
                             Type const* type) {
    if (value->type() == type) {
        return value;
    }
    SC_ASSERT(value->type()->size() == type->size(), "");
    BasicBlockBuilder builder(ctx, insertPoint->parent());
    return builder.insert<ConversionInst>(insertPoint,
                                          value,
                                          type,
                                          Conversion::Bitcast,
                                          "prom.bitcast");
}

static uint64_t extendByteToWord(int64_t value) {
    uint64_t byte = static_cast<uint64_t>(value) & 0xFF;
    uint64_t result = 0;
    for (int i = 0; i < 8; ++i) {
        result |= byte << i * 8;
    }
    return result;
}

/// We will need this function when we promote memsets of value other than zero
[[maybe_unused]] static APInt extendByteToBitWidth(int64_t value,
                                                   size_t bitwidth) {
    size_t numWords = utl::ceil_divide(bitwidth, 64);
    uint64_t const extWord = extendByteToWord(value);
    utl::small_vector<uint64_t> ext(numWords, extWord);
    return APInt(ext, bitwidth);
}

static Type const* getLoadedType(Alloca const* address) {
    auto users = address->users();
    auto loadItr = ranges::find_if(users, isa<Load>);
    if (loadItr != users.end()) {
        return (*loadItr)->type();
    }
    return address->allocatedType();
}

bool VariableInfo::renameDef(Instruction* inst) {
    if (definingAddress(inst) != address) {
        return false;
    }
    if (auto* store = dyncast<Store*>(inst)) {
        genName(store->value());
        return true;
    }
    auto* call = cast<Call*>(inst);
    if (isMemcpy(call)) {
        BasicBlockBuilder builder(ctx, inst->parent());
        auto* source = memcpySource(call);
        auto* value = builder.insert<Load>(call, source, type, "prom.memcpy");
        genName(value);
        return true;
    }
    if (isMemset(call)) {
        BasicBlockBuilder builder(ctx, inst->parent());
        auto* loadedType = getLoadedType(address);
        auto* value = ctx.nullConstant(loadedType);
        genName(value);
        return true;
    }
    SC_UNREACHABLE();
}

bool VariableInfo::renameUse(Instruction* inst) {
    if (usingAddress(inst) != address) {
        return false;
    }
    Value* value = getLastDef();
    if (auto* load = dyncast<Load*>(inst)) {
        /// The stack being empty means we load from
        /// uninitialized memory, so we replace the load
        /// with `undef`
        if (!value) {
            value = ctx.undef(load->type());
        }
        load->replaceAllUsesWith(bitcast(value, load, load->type()));
        return true;
    }
    if (!value) {
        value = ctx.undef(type);
    }
    auto* call = cast<Call*>(inst);
    if (isMemcpy(call)) {
        auto* dest = memcpyDest(call);
        BasicBlockBuilder builder(ctx, inst->parent());
        builder.insert<Store>(call, dest, value);
        return true;
    }
    if (isMemset(call)) {
        auto* dest = memsetDest(call);
        BasicBlockBuilder builder(ctx, inst->parent());
        builder.insert<Store>(call, dest, value);
        return true;
    }
    SC_UNREACHABLE();
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
        renameDef(&inst) || renameUse(&inst);
    }
    for (auto* succ: BB->successors()) {
        if (stack.empty()) {
            continue;
        }
        if (auto* phi = getPhi(succ)) {
            auto* argument = versions[stack.top()];
            phi->setArgument(BB, argument);
            /// This is a preliminary hack to make sure the phi instructions
            /// have the correct type. This does not work if we use memory for
            /// type punning.
            phi->setType(argument->type());
        }
    }
    for (auto* succ: BB->successors()) {
        rename(succ);
    }
    /// We pop the defs in this basic block off the stack
    for (auto& inst: *BB) {
        // clang-format off
        bool isDef = SC_MATCH (inst) {
            [&](Phi const& phi) {
                return &phi == getPhi(BB);
            },
            [&](Store const& store) {
                return address == store.address();
            },
            [&](Call const& call) {
                return address == definingAddress(&call);
            },
            [&](Instruction const& inst) { return false; }
        }; // clang-format on
        if (isDef) {
            stack.pop();
        }
    }
}

void VariableInfo::clean() {
    for (auto* use: uses) {
        use->parent()->erase(use);
    }
    for (auto* def: defs) {
        def->parent()->erase(def);
    }
    for (auto [BB, phi]: BBToPhiMap) {
        if (phi->users().empty()) {
            phi->parent()->erase(phi);
        }
    }
    SC_ASSERT(address->users().empty(), "Should be empty after promotion");
    address->parent()->erase(address);
}
