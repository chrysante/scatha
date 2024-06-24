#include "Opt/Passes.h"

#include <optional>
#include <queue>
#include <span>
#include <unordered_map>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <svm/Builtin.h>
#include <utl/function_view.hpp>
#include <utl/hash.hpp>
#include <utl/hashtable.hpp>
#include <utl/strcat.hpp>
#include <utl/vector.hpp>

#include "Common/Allocator.h"
#include "Common/Ranges.h"
#include "IR/Builder.h"
#include "IR/CFG.h"
#include "IR/Clone.h"
#include "IR/Context.h"
#include "IR/Dominance.h"
#include "IR/Loop.h"
#include "IR/PassRegistry.h"
#include "IR/Print.h"
#include "IR/Type.h"
#include "IR/Validate.h"
#include "Opt/AllocaPromotion.h"
#include "Opt/Common.h"
#include "Opt/MemberTree.h"

using namespace scatha;
using namespace ir;
using namespace opt;
using namespace ranges::views;

SC_REGISTER_PASS(opt::sroa, "sroa", PassCategory::Simplification);

/// Uniform interface to get the associated pointer and type of the load or
/// store instruction \p inst
std::pair<Value const*, Type const*> getLSPointerAndType(
    Instruction const* inst) {
    using Ret = std::pair<Value const*, Type const*>;
    // clang-format off
    return SC_MATCH (*inst) {
        [](Load const& load) {
            return std::pair{ load.address(), load.type() };
        },
        [](Store const& store) {
            return std::pair{ store.address(), store.value()->type() };
        },
        [](Instruction const&) -> Ret { SC_UNREACHABLE(); },
    }; // clang-format on
}

namespace {

/// Stores data that shall be available for the entire duration of the algorithm
struct SROAContext {
    /// We cache all member trees because they are expensive to compute and may
    /// be used several times per type
    std::unordered_map<Type const*, MemberTree> memberTrees;

    /// \Returns the existing member tree for \p type or creates a new one if
    /// necessary
    MemberTree const& getMemberTree(Type const* type) {
        auto itr = memberTrees.find(type);
        if (itr != memberTrees.end()) {
            return itr->second;
        }
        return memberTrees.insert({ type, MemberTree::compute(type) })
            .first->second;
    }
};

/// Represents a subregion of the analyzed alloca
struct AllocaRegion {
    Instruction const* beginPtr;
    size_t size;

    bool operator==(AllocaRegion const&) const = default;
};

} // namespace

template <>
struct std::hash<AllocaRegion> {
    size_t operator()(AllocaRegion const& region) const {
        return utl::hash_combine(region.beginPtr, region.size);
    }
};

namespace {

/// Represents a slice of an alloca instruction. The set of _slices_ of an
/// alloca region is the most fine grained partition necessary to make every
/// access be a an access of one or multiple entire slices. This means after
/// partitioning the alloca according to the slices we can get rid of all GEP
/// instructions. Every slice will be temporarily associated with a new alloca
/// instruction before it gets promoted
struct Slice {
    Slice(size_t begin, size_t end, Alloca* newAlloca):
        _begin(begin), _end(end), _newAlloca(newAlloca) {}

    /// \Returns the index of the first byte of the slice
    size_t begin() const { return _begin; }

    /// \Returns the index of the first byte past the end of the slice
    size_t end() const { return _end; }

    /// \Returns the size of the slice in bytes
    size_t size() const { return end() - begin(); }

    /// \Returns the associated intermediate alloca instruction
    Alloca* newAlloca() const { return _newAlloca; }

    /// Print the slice to ostream \p str
    [[maybe_unused]] friend std::ostream& operator<<(std::ostream& str,
                                                     Slice const& slice) {
        return str << toString(slice.newAlloca()) << " [" << slice.begin()
                   << ", " << slice.end() << ")";
    }

private:
    size_t _begin, _end;
    Alloca* _newAlloca;
};

using Subrange = std::pair<size_t, size_t>;

/// Represents a variable (an alloca instruction) that we are trying to slice
/// and promote. This holds most relevant data of the algorithm
struct Variable {
    SROAContext& sroa;
    Context& ctx;
    Function& function;
    LoopNestingForest& LNF;
    Alloca* baseAlloca;

    /// The global memcpy function. This will be set if any memcpy accessed our
    /// alloca and we keep it here to generate new calls to memcpy
    Callable* memcpy = nullptr;

    /// Global memset function, analogous to memcpy.
    Callable* memset = nullptr;

    /// All instructions (loads, stores, memcpys and memsets) that directly or
    /// indirectly read or write parts of our alloca region
    utl::hashset<Instruction*> accesses;

    /// All the GEPs that compute pointers into our alloca
    utl::hashset<GetElementPointer*> geps;

    /// All the phis that (transitively) use our alloca
    utl::hashset<Phi*> phis;

    /// Maps loads, stores and GEPs to the phi node that they (transitively) get
    /// their pointer from
    utl::hashmap<Instruction*, Phi*> assocPhis;

    /// Maps all pointer instructions to their offset into the alloca region.
    /// This stores `std::optional<size_t>` because for GEPs that derive from
    /// phi nodes we cannot directly compute their offsets. This map is also the
    /// place where we store all instructions that compute pointers into our
    /// alloca.
    utl::hashmap<Instruction const*, std::optional<size_t>> ptrToOffsetMap;

    /// Maps subranges of the alloca region to lists of all slices in that
    /// subrange
    utl::hashmap<Subrange, utl::small_vector<Slice>> subrangeToSlices;

    /// All intermediate alloca instructions created for the slices
    utl::small_vector<Alloca*> insertedAllocas;

    /// Accessors for `ptrToOffsetMap` @{

    /// \Returns the offset of \p ptr into our alloca region. \p ptr must be
    /// registered and have a valid offset
    size_t getPtrOffset(Value const* ptr) const {
        auto result = tryGetPtrOffset(ptr);
        SC_ASSERT(result, "Not found");
        return *result;
    }

    /// \Returns the offset of \p ptr into our alloca region if any offset is
    /// registered
    std::optional<size_t> tryGetPtrOffset(Value const* ptr) const {
        auto* inst = dyncast<Instruction const*>(ptr);
        if (!inst) {
            return std::nullopt;
        }
        auto itr = ptrToOffsetMap.find(inst);
        if (itr != ptrToOffsetMap.end()) {
            return itr->second;
        }
        return std::nullopt;
    }

    /// \Returns `true` if \p ptr is a pointer into our alloca region
    bool isPointerToOurAlloca(Value const* ptr) const {
        return tryGetPtrOffset(ptr).has_value();
    }

    /// Register \p ptr as a pointer into our alloca region
    bool addPointer(Instruction* ptr, std::optional<size_t> offset) {
        return ptrToOffsetMap.insert({ ptr, offset }).second;
    }

    /// Override the stored pointer offset of \p ptr with \p offset
    /// Adds \p ptr if not registered before
    void setPointerOffset(Instruction* ptr, size_t offset) {
        ptrToOffsetMap[ptr] = offset;
    }
    /// @}

    /// Access the slices of the subregion \p region
    std::span<Slice const> getSubslices(Subrange subrange) const {
        auto itr = subrangeToSlices.find(subrange);
        SC_ASSERT(itr != subrangeToSlices.end(), "Not found");
        return itr->second;
    }

    Variable(SROAContext& sroa, Context& ctx, Function& function,
             Alloca* baseAlloca):
        sroa(sroa),
        ctx(ctx),
        function(function),
        LNF(function.getOrComputeLNF()),
        baseAlloca(baseAlloca) {}

    /// \Returns the associated phi instruction or \p inst if \p inst is a phi
    Phi* getAssocPhi(Value* value) const {
        if (auto* phi = dyncast<Phi*>(value)) {
            return phi;
        }
        auto* inst = dyncast<Instruction*>(value);
        auto itr = assocPhis.find(inst);
        if (itr != assocPhis.end()) {
            return itr->second;
        }
        return nullptr;
    }

    /// Adds \p inst to the respective set of instructions
    /// \Returns `true` if this instruction was not added before
    bool memorize(Instruction* inst) {
        // clang-format off
        return SC_MATCH (*inst) {
            [&](Alloca&) { return true; },
            [&](Load& load) { return accesses.insert(&load).second; },
            [&](Store& store) { return accesses.insert(&store).second; },
            [&](Call& call) { return accesses.insert(&call).second; },
            [&](GetElementPointer& gep) { return geps.insert(&gep).second; },
            [&](Phi& phi) { return phis.insert(&phi).second; },
            [&](Instruction&) -> bool { SC_UNREACHABLE(); },
        }; // clang-format on
    }

    /// Removes \p inst from the respective set of instructions
    void forget(Instruction* inst) {
        // clang-format off
        SC_MATCH (*inst) {
            [&](Load& load) { accesses.erase(&load); },
            [&](Store& store) { accesses.erase(&store); },
            [&](Call& call) { accesses.erase(&call); },
            [&](GetElementPointer& gep) { geps.erase(&gep); },
            [&](Phi& phi) { phis.erase(&phi); },
            [&](Instruction&) { SC_UNREACHABLE(); },
        }; // clang-format on
        assocPhis.erase(inst);
    }

    /// Run the algorithm for this variable
    bool run();

    /// # Analysis
    /// We transitively traverse all the users of the alloca to see if we can
    /// slice it
    bool analyze(Instruction* inst);
    bool analyzeUsers(Instruction* inst);

    bool analyzeImpl(Instruction* inst);
    bool analyzeImpl(Alloca* base);
    bool analyzeImpl(Load* load);
    bool analyzeImpl(Store* store);
    bool analyzeImpl(Call* call);
    bool analyzeMemcpy(Call* call);
    bool analyzeMemset(Call* call);
    bool analyzeImpl(GetElementPointer* gep);
    bool analyzeImpl(Phi* phi);

    /// If the pointer \p pointer is derived from a phi instruction, this
    /// function checks if the instruction \p user post dominates the phi, i.e.
    /// control flow starting at the phi instruction must go through \p user. If
    /// \p pointer is not derived from a phi instruction this function always
    /// returns `true`.
    /// We need this check to prevent speculative execution of
    /// stores which are not generally safe.
    /// For example in the following code
    ///
    ///         %p = phi ptr [label %foo: %q], [label %bar: %r]
    ///         branch i1 %cond, label %quux, label %quuz
    ///     %quux:
    ///         store ptr %p, i64 1
    ///
    /// we only store conditionally to the phi'd pointer, so we cannot move the
    /// store to the predecessors of the phi. Here
    /// `pointerUsePostdominatesPhi(<StoreInst>, %p)` returns false so we know
    /// the phi is not rewritable
    bool pointerUsePostdominatesPhi(Instruction* user, Value* pointer);

    /// If the pointer \p pointer is derived from a phi instruction, this
    /// function checks if the value \p value dominates the phi, i.e.
    /// control flow starting at \p value must go through the phi instruction.
    /// If \p pointer is not derived from a phi instruction or \p value is not
    /// an instruction, this function always returns `true`. This functions is
    /// needed to guard phi rewrites. For example in the following code
    ///
    ///     %p = phi ptr [label %foo: %q], [label %bar: %r]
    ///     %value = mul i64 ...
    ///     store ptr %p, i64 %value
    ///
    /// we cannot move the store to `%p` to the predecessors of the phi
    /// instruction because the stored value is defined after the phi. Here the
    /// test `valueStrictlyDominatesPhi(%value, %p)` returns false so we know
    /// that this phi is not rewritable
    bool valueStrictlyDominatesPhi(Value* value, Value* pointer);

    /// If any phi instructions transitively use the alloca we copy the users of
    /// the phi into each of the predecessor blocks of the phi and add new phi
    /// instructions if necessary. The analyze step makes sure that this
    /// operation is safe. After this step we can erase all phis that use the
    /// alloca
    bool rewritePhis();

    /// Clones the instruction \p inst and inserts it before \p insertBefore
    /// \Returns a pointer to the clone of \p inst
    Instruction* copyInstruction(Instruction* insertBefore, Instruction* inst);

    /// We slice the alloca based on the load and store instructions that
    /// (transitively) use the alloca
    bool computeSlices();
    utl::small_vector<Subrange, 2> getAccessedSubranges(
        Instruction const* inst);

    /// We replace loads and stores with multiple loads and stores to the slices
    /// if necessary
    bool replaceBySlices();
    bool replaceBySlices(Load* load);
    bool replaceBySlices(Store* store);
    bool replaceBySlices(Call* call);
    bool replaceMemcpyBySlices(Call* call);
    bool replaceMemcpyBySlicesWithin(Call* call);
    bool replaceMemcpyBySlicesDest(Call* call);
    bool replaceMemcpyBySlicesSource(Call* call);
    bool replaceMemsetBySlices(Call* call);
    bool replaceBySlices(Instruction*) { SC_UNREACHABLE(); }

    /// We try to promote all the slices
    bool promoteSlices();
};

} // namespace

bool opt::sroa(Context& ctx, Function& function) {
    SROAContext sroaCtx;
    auto worklist = function.entry() | Filter<Alloca> | TakeAddress |
                    ToSmallVector<>;
    bool modified = false;
    /// We run the algorithm for each alloca. If an alloca is sliced we remove
    /// it from the worklist. We continuously iterate the worklist until we
    /// can't slice anything anymore. We need to traverse the worklist multiple
    /// times to ensure idempotency of the pass because pointers to allocas
    /// might be stored to other allocas and only become promoteable once the
    /// other alloca has been promoted.
    while (!worklist.empty()) {
        bool thisRound = false;
        for (auto itr = worklist.begin(); itr != worklist.end(); ++itr) {
            auto* baseAlloca = *itr;
            if (Variable(sroaCtx, ctx, function, baseAlloca).run()) {
                thisRound = true;
                *itr = worklist.back();
                worklist.pop_back();
                --itr;
            }
        }
        if (!thisRound) {
            break;
        }
        modified = true;
    }
    ir::assertInvariants(ctx, function);
    return modified;
}

bool Variable::run() {
    if (!analyze(baseAlloca)) {
        return false;
    }
    bool modified = false;
    modified |= rewritePhis();
    modified |= computeSlices();
    modified |= replaceBySlices();
    modified |= promoteSlices();
    return modified;
}

bool Variable::analyze(Instruction* inst) {
    return visit(*inst, [&](auto& inst) { return analyzeImpl(&inst); });
}

bool Variable::analyzeUsers(Instruction* inst) {
    return ranges::all_of(inst->users(), [&](auto* user) {
        if (auto* phi = getAssocPhi(inst)) {
            assocPhis[user] = phi;
        }
        return analyze(user);
    });
}

bool Variable::analyzeImpl(Instruction*) { return false; }

bool Variable::analyzeImpl(Alloca* allocaInst) {
    SC_EXPECT(allocaInst == baseAlloca);
    if (!isa<IntegralConstant>(allocaInst->count())) {
        return false;
    }
    addPointer(allocaInst, 0);
    return analyzeUsers(allocaInst);
}

bool Variable::analyzeImpl(Load* load) {
    /// TODO: Evaluate if this is really needed for loads
    if (!pointerUsePostdominatesPhi(load, load->address())) {
        return false;
    }
    memorize(load);
    return true;
}

bool Variable::analyzeImpl(Store* store) {
    /// If we store any pointer into the alloca to memory it escapes our
    /// analysis
    if (isPointerToOurAlloca(store->value())) {
        return false;
    }
    if (!pointerUsePostdominatesPhi(store, store->address())) {
        return false;
    }
    if (!valueStrictlyDominatesPhi(store->value(), store->address())) {
        return false;
    }
    memorize(store);
    return true;
}

bool Variable::analyzeImpl(Call* call) {
    if (isConstSizeMemcpy(call)) {
        return analyzeMemcpy(call);
    }
    if (isConstMemset(call)) {
        return analyzeMemset(call);
    }
    return false;
}

bool Variable::analyzeMemcpy(Call* call) {
    auto* allocaPtr = [&]() -> Value* {
        if (auto* dest = memcpyDest(call); isPointerToOurAlloca(dest)) {
            return dest;
        }
        if (auto* source = memcpySource(call); isPointerToOurAlloca(source)) {
            return source;
        }
        SC_UNREACHABLE();
    }();
    if (!pointerUsePostdominatesPhi(call, allocaPtr)) {
        return false;
    }
    bool otherArgDomPhi = ranges::all_of(call->arguments(), [&](Value* arg) {
        return arg == allocaPtr || valueStrictlyDominatesPhi(arg, allocaPtr);
    });
    if (!otherArgDomPhi) {
        return false;
    }
    memcpy = cast<Callable*>(call->function());
    memorize(call);
    return true;
}

bool Variable::analyzeMemset(Call* call) {
    auto* dest = memsetDest(call);
    if (!isPointerToOurAlloca(dest)) {
        return false;
    }
    if (!pointerUsePostdominatesPhi(call, dest)) {
        return false;
    }
    /// For memset we don't need to check if the other args dominate the phi
    /// because we only consider const memsets which means all arguments but
    /// `dest` are constants
    memset = cast<Callable*>(call->function());
    memorize(call);
    return true;
}

bool Variable::analyzeImpl(GetElementPointer* gep) {
    if (!gep->hasConstantArrayIndex()) {
        return false;
    }
    /// If our base pointer is a phi node we can't compute the constant offset
    /// here, because the arguments of the phi node may have different offsets.
    if (isa<Phi>(gep->basePointer())) {
        addPointer(gep, std::nullopt);
    }
    else {
        size_t offset =
            getPtrOffset(gep->basePointer()) + *gep->constantByteOffset();
        addPointer(gep, offset);
    }
    if (memorize(gep)) {
        return analyzeUsers(gep);
    }
    return true;
}

bool Variable::analyzeImpl(Phi* phi) {
    /// We cannot slice the alloca if we compute pointers to it through a loop
    if (LNF[phi->parent()]->isProperLoop()) {
        return false;
    }
    addPointer(phi, size_t(-1));
    if (memorize(phi)) {
        return analyzeUsers(phi);
    }
    return true;
}

bool Variable::pointerUsePostdominatesPhi(Instruction* user, Value* ptr) {
    auto* phi = getAssocPhi(ptr);
    if (!phi) {
        return true;
    }
    auto& domInfo = function.getOrComputePostDomInfo();
    auto& dominatorSet = domInfo.dominatorSet(phi->parent());
    return dominatorSet.contains(user->parent());
}

bool Variable::valueStrictlyDominatesPhi(Value* value, Value* ptr) {
    auto* inst = dyncast<Instruction*>(value);
    if (!inst) {
        return true;
    }
    auto* phi = getAssocPhi(ptr);
    if (!phi) {
        return true;
    }
    if (phi->parent() == inst->parent()) {
        return false;
    }
    auto& domInfo = function.getOrComputeDomInfo();
    auto& dominatorSet = domInfo.dominatorSet(phi->parent());
    return dominatorSet.contains(inst->parent());
}

/// FIXME: These functions are generic and have little to do with SROA. Move
/// them to Opt/Common
[[maybe_unused]] static void forwardBFS(
    Function& function, utl::function_view<void(BasicBlock*)> fn) {
    std::queue<BasicBlock*> queue;
    queue.push(&function.entry());
    utl::hashset<BasicBlock*> visited{ &function.entry() };
    while (!queue.empty()) {
        auto* BB = queue.front();
        queue.pop();
        fn(BB);
        for (auto* succ: BB->successors()) {
            if (visited.insert(succ).second) {
                queue.push(succ);
            }
        }
    }
}

[[maybe_unused]] static void reverseBFS(
    Function& function, utl::function_view<void(BasicBlock*)> fn) {
    auto visited = function | TakeAddress | ranges::views::filter([](auto* BB) {
        return isa<Return>(BB->terminator());
    }) | ranges::to<utl::hashset<BasicBlock*>>;
    std::queue<BasicBlock*> queue;
    for (auto* exit: visited) {
        queue.push(exit);
    }
    while (!queue.empty()) {
        auto* BB = queue.front();
        queue.pop();
        fn(BB);
        for (auto* pred: BB->predecessors()) {
            if (visited.insert(pred).second) {
                queue.push(pred);
            }
        }
    }
}

bool Variable::rewritePhis() {
    if (phis.empty()) {
        return false;
    }
    /// We split critical edges so we can safely copy users of phi instructions
    /// to predecessors of the phis without executing any instructions
    /// speculatively
    /// TODO: We only need to split the edges along which we want to move
    /// instructions, so split lazily
    splitCriticalEdges(ctx, function);
    utl::small_vector<Instruction*> toErase;
    utl::hashmap<std::pair<BasicBlock*, Value*>, Instruction*> toCopyMap;
    auto getInsertPoint = [map = utl::hashmap<BasicBlock*, Instruction*>{}](
                              BasicBlock* BB) mutable -> Instruction*& {
        auto& result = map[BB];
        if (!result) {
            result = BB->terminator();
        }
        return result;
    };
    reverseBFS(function, [&](BasicBlock* BB) {
        struct PhiInsertion {
            Phi* before;
            UniquePtr<Phi> inserted;
        };
        /// We batch the phi insertions to be able to traverse the block without
        /// iterator invalidations
        utl::small_vector<PhiInsertion> phiInsertions;
        for (auto& inst: *BB | ranges::views::reverse |
                             Filter<Load, Store, GetElementPointer, Call>)
        {
            auto* phi = getAssocPhi(&inst);
            if (!phi) {
                continue;
            }
            /// If the associated phi only has one argument, we don't need to
            /// make a copy, as we can directly use that argument in our
            /// instruction. It is also necessary to not make a copy in this
            /// case, because our predecessor might have multiple successors and
            /// we would execute this instruction speculatively in the
            /// predecessor
            if (phi->operands().size() == 1) {
                auto* phiArgument = phi->operandAt(0);
                inst.tryUpdateOperand(phi, phiArgument);
                if (auto* assocPhi = getAssocPhi(phiArgument)) {
                    assocPhis[&inst] = assocPhi;
                }
                if (auto* gep = dyncast<GetElementPointer*>(&inst)) {
                    if (auto baseOffset = tryGetPtrOffset(gep->basePointer())) {
                        size_t offset =
                            *baseOffset + *gep->constantByteOffset();
                        setPointerOffset(&inst, offset);
                    }
                }
                continue;
            }
            /// We look at all instructions that have an associated phi node.
            /// We make copies of the instructions in each of the predecessor
            /// blocks of the phi
            utl::small_vector<PhiMapping> newPhiArgs;
            for (auto [pred, phiArgument]: phi->arguments()) {
                SC_ASSERT(pred->numSuccessors() == 1,
                          "This is guaranteed, because we have split the "
                          "critical edges and don't make copies for phis that "
                          "only have one argument.");
                auto& insertPoint = getInsertPoint(pred);
                auto* copy = copyInstruction(insertPoint, &inst);
                insertPoint = copy;
                toCopyMap[{ pred, &inst }] = copy;
                if (isPointerToOurAlloca(phiArgument)) {
                    memorize(copy);
                }
                for (auto [index, operand]:
                     copy->operands() | ranges::views::enumerate)
                {
                    if (operand == phi) {
                        copy->setOperand(index, phiArgument);
                        continue;
                    }
                    auto itr = toCopyMap.find({ pred, operand });
                    if (itr != toCopyMap.end()) {
                        copy->setOperand(index, itr->second);
                    }
                }
                newPhiArgs.push_back({ pred, copy });
                if (auto* assocPhi = getAssocPhi(phiArgument)) {
                    assocPhis[copy] = assocPhi;
                }
                if (auto* gep = dyncast<GetElementPointer*>(copy)) {
                    if (auto baseOffset = tryGetPtrOffset(gep->basePointer())) {
                        addPointer(gep,
                                   *baseOffset + *gep->constantByteOffset());
                    }
                }
            }
            /// If the instruction is a load we phi the copied loads together
            /// We also prune a little bit here to avoid adding unused phi nodes
            if (isa<Load>(inst) && !inst.unused()) {
                auto newPhi =
                    allocate<Phi>(newPhiArgs, utl::strcat(inst.name(), ".phi"));
                inst.replaceAllUsesWith(newPhi.get());
                phiInsertions.push_back(
                    { .before = phi, .inserted = std::move(newPhi) });
            }
            toErase.push_back(&inst);
        }
        /// After traversing one basic block we insert all the added phi
        /// instructions
        for (auto& [before, inserted]: phiInsertions) {
            before->parent()->insert(before, inserted.release());
        }
    });
    for (auto* inst: toErase) {
        forget(inst);
        inst->parent()->erase(inst);
    }
    /// At this stage the phi nodes are only used by other phi nodes and
    /// we erase all of them
    for (auto* phi: phis) {
        SC_ASSERT(ranges::all_of(phi->users(), isa<Phi>),
                  "All users of the phis must be other phis at this point");
        phi->parent()->erase(phi);
    }
    phis.clear();
    return true;
}

Instruction* Variable::copyInstruction(Instruction* insertBefore,
                                       Instruction* inst) {
    auto* copy = clone(ctx, inst).release();
    insertBefore->parent()->insert(insertBefore, copy);
    return copy;
}

static utl::small_vector<Slice> slicesInRange(size_t begin, size_t end,
                                              std::span<Slice const> slices) {
    return ranges::make_subrange(ranges::lower_bound(slices, begin,
                                                     ranges::less{},
                                                     &Slice::begin),
                                 ranges::upper_bound(slices, end,
                                                     ranges::less{},
                                                     &Slice::end)) |
           ranges::views::transform([&](Slice slice) {
        return Slice(slice.begin() - begin, slice.end() - begin,
                     slice.newAlloca());
    }) | ToSmallVector<>;
}

bool Variable::computeSlices() {
    utl::hashset<size_t> set;
    /// We insert all the slice points at the positions that we directly load
    /// from and store to
    for (auto* inst: accesses) {
        for (auto subrange: getAccessedSubranges(inst)) {
            auto [begin, end] = subrange;
            set.insert(end);
            set.insert(begin);
        }
    }
    /// Then we insert all the slice points at "critical positions".
    /// If we slice at a certain member offset, we also need to slice the alloca
    /// at all offsets of siblings in the member tree of that node to be able to
    /// store all siblings
    for (auto* inst: accesses) {
        /// Calls to memcpy and memset don't define critical positions because
        /// they have no structure
        if (isa<Call>(inst)) {
            continue;
        }
        auto [pointer, type] = getLSPointerAndType(inst);
        size_t const offset = getPtrOffset(pointer);
        auto& tree = sroa.getMemberTree(type);
        auto DFS = [&](auto& DFS, MemberTree::Node const* node) -> void {
            for (auto* child: node->children()) {
                DFS(DFS, child);
            }
            bool hasSlicePoints = false;
            for (auto* child: node->children()) {
                hasSlicePoints |= child->begin() != node->begin() &&
                                  set.contains(offset + child->begin());
                hasSlicePoints |= child->end() != node->end() &&
                                  set.contains(offset + child->end());
            }

            if (hasSlicePoints) {
                for (auto* child: node->children()) {
                    set.insert(offset + child->begin());
                    set.insert(offset + child->end());
                }
            }
        };
        DFS(DFS, tree.root());
    }
    auto sortedSet = set | ToSmallVector<>;
    ranges::sort(sortedSet);
    auto primSlices =
        ranges::views::zip(sortedSet, sortedSet | ranges::views::drop(1));
    utl::small_vector<Slice> slices;
    slices.reserve(primSlices.size());
    bool modified = false;
    for (auto [begin, end]: primSlices) {
        Alloca* newAlloca = baseAlloca;
        if (begin != 0 || end != baseAlloca->allocatedSize().value()) {
            modified = true;
            BasicBlockBuilder builder(ctx, &function.entry());
            newAlloca = builder.insert<Alloca>(baseAlloca,
                                               ctx.intConstant(end - begin, 32),
                                               ctx.intType(8),
                                               utl::strcat(baseAlloca->name(),
                                                           ".slice"));
            insertedAllocas.push_back(newAlloca);
        }
        slices.push_back({ begin, end, newAlloca });
    }
    for (auto* inst: accesses) {
        for (auto subrange: getAccessedSubranges(inst)) {
            auto [begin, end] = subrange;
            subrangeToSlices[subrange] = slicesInRange(begin, end, slices);
        }
    }
    return modified;
}

utl::small_vector<Subrange, 2> Variable::getAccessedSubranges(
    Instruction const* inst) {
    using Ret = utl::small_vector<Subrange, 2>;
    // clang-format off
    return SC_MATCH (*inst) {
        [&](Load const& load) -> Ret {
            size_t offset = getPtrOffset(load.address());
            return { { offset, offset + load.type()->size() } };
        },
        [&](Store const& store) -> Ret {
            size_t offset = getPtrOffset(store.address());
            return { { offset, offset + store.value()->type()->size() } };
        },
        [&](Call const& call) -> Ret {
            if (isMemcpy(&call)) {
                Ret result;
                if (auto offset = tryGetPtrOffset(memcpyDest(&call))) {
                    result.push_back({ *offset, *offset + memcpySize(&call) });
                }
                if (auto offset = tryGetPtrOffset(memcpySource(&call))) {
                    result.push_back({ *offset, *offset + memcpySize(&call) });
                }
                return result;
            }
            if (isMemset(&call)) {
                size_t offset = getPtrOffset(memsetDest(&call));
                return { { offset, offset + memsetSize(&call) } };
            }
            SC_UNREACHABLE();
        },
        [&](Instruction const&) -> Ret {
            SC_UNREACHABLE();
        }
    }; // clang-format on
}

bool Variable::replaceBySlices() {
    bool modified = false;
    for (auto* inst: accesses) {
        modified |=
            visit(*inst, [&](auto& inst) { return replaceBySlices(&inst); });
    }
    return modified;
}

static void memTreePostorder(
    MemberTree const& tree, std::span<Slice const> slices,
    utl::function_view<void(MemberTree::Node const*, std::span<Slice const>,
                            std::span<size_t const>)>
        fn) {
    utl::small_vector<size_t> indices;
    auto sliceItr = slices.begin();
    auto impl = [&](auto& impl, auto& sliceItr,
                    MemberTree::Node const* node) -> bool {
        bool calledAnyChildren = false, calledAllChildren = true;
        auto childItr = sliceItr;
        for (auto* child: node->children()) {
            indices.push_back(child->index());
            bool called = impl(impl, childItr, child);
            calledAnyChildren |= called;
            calledAllChildren &= called;
            indices.pop_back();
        }
        if (calledAnyChildren) {
            SC_ASSERT(calledAllChildren, "Need to call either all or none");
            return true;
        }
        while (sliceItr != slices.end() && sliceItr->begin() < node->begin()) {
            ++sliceItr;
        }
        auto begin = sliceItr;
        while (sliceItr != slices.end() && sliceItr->end() <= node->end()) {
            ++sliceItr;
        }
        fn(node, std::span(begin, sliceItr), indices);
        return begin != sliceItr;
    };
    impl(impl, sliceItr, tree.root());
}

bool Variable::replaceBySlices(Load* load) {
    auto& tree = sroa.getMemberTree(load->type());
    auto slices = getSubslices(getAccessedSubranges(load).front());
    bool modified = false;
    Value* aggregate = ctx.undef(load->type());
    memTreePostorder(tree, slices,
                     [&](MemberTree::Node const* node,
                         std::span<Slice const> slices,
                         std::span<size_t const> indices) {
        switch (slices.size()) {
        case 0:
            break;
        case 1: {
            auto slice = slices.front();
            SC_ASSERT(slice.begin() == node->begin() &&
                          slice.end() == node->end(),
                      "Implement this case!");
            if (indices.empty()) {
                load->setAddress(slice.newAlloca());
            }
            else {
                BasicBlockBuilder builder(ctx, load->parent());
                auto* newLoad = builder.insert<Load>(load, slice.newAlloca(),
                                                     node->type(),
                                                     std::string(load->name()));
                aggregate = builder.insert<InsertValue>(load, aggregate,
                                                        newLoad, indices,
                                                        "sroa.insert");
                modified = true;
            }
            break;
        }
        default: {
            Value* value = ctx.intConstant(0, node->type()->size() * 8);
            auto* intType = value->type();
            BasicBlockBuilder builder(ctx, load->parent());
            builder.setAddPoint(load);
            for (auto slice: slices) {
                Value* sliceValue =
                    builder.add<Load>(slice.newAlloca(),
                                      ctx.intType(slice.size() * 8),
                                      std::string(load->name()));
                sliceValue = builder.add<ConversionInst>(sliceValue, intType,
                                                         Conversion::Zext,
                                                         "sroa.zext");
                SC_ASSERT(slice.begin() >= node->begin(), "");
                auto localSliceBegin = slice.begin() - node->begin();
                if (localSliceBegin > 0) {
                    sliceValue = builder.add<ArithmeticInst>(
                        sliceValue, ctx.intConstant(localSliceBegin * 8, 32),
                        ArithmeticOperation::LShL, "sroa.shift");
                }
                value = builder.add<ArithmeticInst>(value, sliceValue,
                                                    ArithmeticOperation::Or,
                                                    "sroa.or");
            }
            if (node->type() != intType) {
                value = builder.add<ConversionInst>(value, node->type(),
                                                    Conversion::Bitcast,
                                                    "sroa.bitcast");
            }
            if (indices.empty()) {
                aggregate = value;
            }
            else {
                aggregate = builder.add<InsertValue>(aggregate, value, indices,
                                                     "sroa.insert");
            }
            modified = true;
            break;
        }
        }
    });
    if (modified) {
        load->replaceAllUsesWith(aggregate);
        load->parent()->erase(load);
    }
    return modified;
}

bool Variable::replaceBySlices(Store* store) {
    auto& tree = sroa.getMemberTree(store->value()->type());
    auto slices = getSubslices(getAccessedSubranges(store).front());
    bool modified = false;
    memTreePostorder(tree, slices,
                     [&](MemberTree::Node const* node,
                         std::span<Slice const> slices,
                         std::span<size_t const> indices) {
        switch (slices.size()) {
        case 0:
            break;
        case 1: {
            auto slice = slices.front();
            SC_ASSERT(slice.begin() == node->begin() &&
                          slice.end() == node->end(),
                      "Implement this case!");
            if (indices.empty()) {
                store->setAddress(slice.newAlloca());
            }
            else {
                BasicBlockBuilder builder(ctx, store->parent());
                auto* extr = builder.insert<ExtractValue>(store, store->value(),
                                                          indices,
                                                          "sroa.extract");
                builder.insert<Store>(store, slice.newAlloca(), extr);
                modified = true;
            }
            break;
        }
        default: {
            BasicBlockBuilder builder(ctx, store->parent());
            builder.setAddPoint(store);
            Value* value = store->value();
            auto* intType = ctx.intType(value->type()->size() * 8);
            if (value->type() != intType) {
                value = builder.add<ConversionInst>(value, intType,
                                                    Conversion::Bitcast,
                                                    "sroa.bitcast");
            }
            for (auto slice: slices) {
                SC_ASSERT(slice.begin() >= node->begin(), "");
                auto localSliceBegin = slice.begin() - node->begin();
                Value* sliceValue = value;
                if (localSliceBegin > 0) {
                    sliceValue = builder.add<ArithmeticInst>(
                        sliceValue, ctx.intConstant(localSliceBegin * 8, 32),
                        ArithmeticOperation::LShR, "sroa.shift");
                }
                sliceValue =
                    builder.add<ConversionInst>(sliceValue,
                                                ctx.intType(slice.size() * 8),
                                                Conversion::Trunc,
                                                "sroa.trunc");
                builder.add<Store>(slice.newAlloca(), sliceValue);
                modified = true;
            }
            break;
        }
        }
    });
    if (modified) {
        store->parent()->erase(store);
    }
    return modified;
}

bool Variable::replaceBySlices(Call* call) {
    if (isMemcpy(call)) {
        return replaceMemcpyBySlices(call);
    }
    if (isMemset(call)) {
        return replaceMemsetBySlices(call);
    }
    SC_UNREACHABLE();
}

bool Variable::replaceMemcpyBySlices(Call* call) {
    auto* dest = memcpyDest(call);
    auto* source = memcpySource(call);
    SC_ASSERT(isPointerToOurAlloca(dest) || isPointerToOurAlloca(source),
              "One of them must point to our alloca");
    if (isPointerToOurAlloca(dest) && isPointerToOurAlloca(source)) {
        return replaceMemcpyBySlicesWithin(call);
    }
    else if (isPointerToOurAlloca(dest)) {
        return replaceMemcpyBySlicesDest(call);
    }
    else /* isPointerToOurAlloca(source) */ {
        return replaceMemcpyBySlicesSource(call);
    }
}

bool Variable::replaceMemcpyBySlicesWithin(Call* call) {
    BasicBlockBuilder builder(ctx, call->parent());
    auto subranges = getAccessedSubranges(call);
    SC_ASSERT(subranges.size() == 2, "");
    auto destRange = subranges[0];
    auto sourceRange = subranges[1];
    auto destSlices = getSubslices(destRange);
    auto sourceSlices = getSubslices(sourceRange);
    SC_ASSERT(!destSlices.empty(), "");
    SC_ASSERT(destSlices.size() == sourceSlices.size(), "");
    if (destSlices.size() == 1) {
        setMemcpyDest(call, destSlices.front().newAlloca());
        setMemcpySource(call, sourceSlices.front().newAlloca());
        return false;
    }
    for (auto [destSlice, sourceSlice]: zip(destSlices, sourceSlices)) {
        SC_ASSERT(destSlice.size() == sourceSlice.size(), "");
        auto* size = ctx.intConstant(destSlice.size(), 64);
        Value* args[] = { destSlice.newAlloca(), size, sourceSlice.newAlloca(),
                          size };
        SC_ASSERT(memcpy, "Must be set to generate call to memcpy");
        builder.insert<Call>(call, memcpy, std::span(args));
    }
    call->parent()->erase(call);
    return true;
}

bool Variable::replaceMemcpyBySlicesDest(Call* call) {
    BasicBlockBuilder builder(ctx, call->parent());
    auto subranges = getAccessedSubranges(call);
    SC_ASSERT(subranges.size() == 1, "");
    auto* byteType = ctx.intType(8);
    auto* source = memcpySource(call);
    auto slices = getSubslices(subranges.front());
    SC_ASSERT(!slices.empty(), "");
    if (slices.size() == 1) {
        setMemcpyDest(call, slices.front().newAlloca());
        return false;
    }
    for (auto slice: slices) {
        auto* gepIndex = ctx.intConstant(slice.begin(), 32);
        auto* sourceSlicePtr =
            builder.insert<GetElementPointer>(call, byteType, source, gepIndex,
                                              std::array<size_t, 0>{},
                                              "sroa.gep");
        auto* size = ctx.intConstant(slice.end() - slice.begin(), 64);
        Value* args[] = { slice.newAlloca(), size, sourceSlicePtr, size };
        SC_ASSERT(memcpy, "Must be set to generate call to memcpy");
        builder.insert<Call>(call, memcpy, std::span(args));
    }
    call->parent()->erase(call);
    return true;
}

bool Variable::replaceMemcpyBySlicesSource(Call* call) {
    BasicBlockBuilder builder(ctx, call->parent());
    auto subranges = getAccessedSubranges(call);
    SC_ASSERT(subranges.size() == 1, "");
    auto* byteType = ctx.intType(8);
    auto* dest = memcpyDest(call);
    auto slices = getSubslices(subranges.front());
    SC_ASSERT(!slices.empty(), "");
    if (slices.size() == 1) {
        setMemcpySource(call, slices.front().newAlloca());
        return false;
    }
    for (auto slice: slices) {
        auto* gepIndex = ctx.intConstant(slice.begin(), 32);
        auto* destSlicePtr =
            builder.insert<GetElementPointer>(call, byteType, dest, gepIndex,
                                              std::array<size_t, 0>{},
                                              "sroa.gep");
        auto* size = ctx.intConstant(slice.end() - slice.begin(), 64);
        Value* args[] = { destSlicePtr, size, slice.newAlloca(), size };
        SC_ASSERT(memcpy, "Must be set to generate call to memcpy");
        builder.insert<Call>(call, memcpy, std::span(args));
    }
    call->parent()->erase(call);
    return true;
}

bool Variable::replaceMemsetBySlices(Call* call) {
    BasicBlockBuilder builder(ctx, call->parent());
    auto subranges = getAccessedSubranges(call);
    auto slices = getSubslices(subranges.front());
    SC_ASSERT(!slices.empty(), "");
    if (slices.size() == 1) {
        setMemsetDest(call, slices.front().newAlloca());
        return false;
    }
    for (auto slice: slices) {
        auto* size = ctx.intConstant(slice.end() - slice.begin(), 64);
        Value* args[] = { slice.newAlloca(), size, memsetValue(call) };
        SC_ASSERT(memset, "Must be set to generate call to memset");
        builder.insert<Call>(call, memset, std::span(args));
    }
    call->parent()->erase(call);
    return true;
}

bool Variable::promoteSlices() {
    bool modified = false;
    for (auto* gep: geps) {
        modified = true;
        gep->parent()->erase(gep);
    }
    auto const& domInfo = function.getOrComputeDomInfo();
    for (auto* newAlloca: insertedAllocas) {
        if (newAlloca == baseAlloca) {
            continue;
        }
        modified |= tryPromoteAlloca(newAlloca, ctx, domInfo);
    }
    modified |= tryPromoteAlloca(baseAlloca, ctx, domInfo);
    return modified;
}
