#include "Opt/Passes.h"

#include <optional>
#include <queue>
#include <span>
#include <unordered_map>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <utl/function_view.hpp>
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
#include "IR/Type.h"
#include "IR/Validate.h"
#include "Opt/AllocaPromotion.h"
#include "Opt/Common.h"
#include "Opt/MemberTree.h"
#include "Opt/PassRegistry.h"

#include "IR/Print.h"
#include <iostream>
#include <termfmt/termfmt.h>

using namespace scatha;
using namespace ir;
using namespace opt;

SC_REGISTER_PASS(opt::sroa2, "sroa2");

namespace {

///
///
///
struct SROAContext {
    ///
    std::unordered_map<Type const*, MemberTree> memberTrees;

    MemberTree const& getMemberTree(Type const* type) {
        auto itr = memberTrees.find(type);
        if (itr != memberTrees.end()) {
            return itr->second;
        }
        return memberTrees.insert({ type, MemberTree::compute(type) })
            .first->second;
    }
};

///
///
///
struct Slice {
    Slice(size_t begin, size_t end, Alloca* newAlloca):
        _begin(begin), _end(end), _newAlloca(newAlloca) {}

    ///
    size_t begin() const { return _begin; }

    ///
    size_t end() const { return _end; }

    ///
    size_t size() const { return end() - begin(); }

    ///
    Alloca* newAlloca() const { return _newAlloca; }

    friend std::ostream& operator<<(std::ostream& str, Slice const& slice) {
        return str << toString(slice.newAlloca()) << " [" << slice.begin()
                   << ", " << slice.end() << ")";
    }

private:
    size_t _begin, _end;
    Alloca* _newAlloca;
};

///
///
///
struct Variable {
    SROAContext& sroa;
    Context& ctx;
    Function& function;
    LoopNestingForest& LNF;
    Alloca* baseAlloca;

    /// All the loads and stores that directly or indirectly use our alloca
    utl::hashset<Instruction*> loadsAndStores;

    /// All the GEPs that compute pointers into our alloca
    utl::hashset<GetElementPointer*> geps;

    /// All the phis that (transitively) use our alloca
    utl::hashset<Phi*> phis;

    /// Maps loads, stores and geps to the phi node that they (transitively) get
    /// their pointer from
    utl::hashmap<Instruction*, Phi*> assocPhis;

    /// Maps all pointer instructions to their offset into the alloca region
    utl::hashmap<Instruction const*, size_t> ptrToOffsetMap;

    /// Maps load and store instructions to a range of slices that it should
    /// load from or store to
    utl::hashmap<Instruction const*, utl::small_vector<Slice>> instToSlicesMap;

    /// All intermediate alloca instructions created for our slices
    utl::small_vector<Alloca*> insertedAllocas;

    /// Accessors for `ptrToOffsetMap` @{
    size_t getPtrOffset(Value const* ptr) const {
        auto result = tryGetPtrOffset(ptr);
        SC_ASSERT(result, "Not found");
        return *result;
    }

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

    bool isPointerToOurAlloca(Value const* ptr) const {
        return tryGetPtrOffset(ptr).has_value();
    }

    bool addPointer(Instruction* ptr, size_t offset) {
        return ptrToOffsetMap.insert({ ptr, offset }).second;
    }
    /// @}

    std::span<Slice const> getSlices(Instruction const* inst) const {
        auto itr = instToSlicesMap.find(inst);
        SC_ASSERT(itr != instToSlicesMap.end(), "Not found");
        return itr->second;
    }

    Variable(SROAContext& sroa,
             Context& ctx,
             Function& function,
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

    bool memorize(Instruction* inst) {
        // clang-format off
        return SC_MATCH (*inst) {
            [&](Alloca& inst) { return true; },
            [&](Load& load) { return loadsAndStores.insert(&load).second; },
            [&](Store& store) { return loadsAndStores.insert(&store).second; },
            [&](GetElementPointer& gep) { return geps.insert(&gep).second; },
            [&](Phi& phi) { return phis.insert(&phi).second; },
            [&](Instruction& inst) -> bool { SC_UNREACHABLE(); },
        }; // clang-format on
    }

    void forget(Instruction* inst) {
        // clang-format off
        SC_MATCH (*inst) {
            [&](Load& load) { loadsAndStores.erase(&load); },
            [&](Store& store) { loadsAndStores.erase(&store); },
            [&](GetElementPointer& gep) { geps.erase(&gep); },
            [&](Phi& phi) { phis.erase(&phi); },
            [&](Instruction& inst) { SC_UNREACHABLE(); },
        }; // clang-format on
        assocPhis.erase(inst);
    }

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
    bool analyzeImpl(GetElementPointer* gep);
    bool analyzeImpl(Phi* phi);

    /// If any phi instructions transitively use the alloca we copy the users of
    /// the phi into each of the predecessor blocks of the phi and add new phi
    /// instructions if necessary. The analyze step makes sure that this
    /// operation is safe. After this step we can erase all phis that use the
    /// alloca
    bool rewritePhis();
    Instruction* copyInstruction(Instruction* inst, BasicBlock* dest);

    ///
    bool computeSlices();
    std::pair<size_t, size_t> getRange(Instruction const* inst);

    ///
    bool replaceBySlices();
    bool replaceBySlices(Load* load);
    bool replaceBySlices(Store* store);
    bool replaceBySlices(Instruction* inst) { SC_UNREACHABLE(); }

    ///
    bool promoteSlices();
};

} // namespace

bool opt::sroa2(Context& ctx, Function& function) {
    SROAContext sroaCtx;
    auto worklist =
        function.entry() | Filter<Alloca> | TakeAddress | ToSmallVector<>;
    bool modified = false;
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

bool Variable::analyzeImpl(Instruction* inst) { return false; }

bool Variable::analyzeImpl(Alloca* allocaInst) {
    SC_ASSERT(allocaInst == baseAlloca, "");
    if (!isa<IntegralConstant>(allocaInst->count())) {
        return false;
    }
    addPointer(allocaInst, 0);
    return analyzeUsers(allocaInst);
}

bool Variable::analyzeImpl(Load* load) {
    memorize(load);
    return true;
}

bool Variable::analyzeImpl(Store* store) {
    /// If we store any pointer into the alloca it escapes our analysis
    if (ptrToOffsetMap.contains(store->value())) {
        return false;
    }
    memorize(store);
    return true;
}

static size_t computeGepOffset(GetElementPointer const* gep) {
    Type const* currentType = gep->inboundsType();
    size_t offset = 0;
    offset += currentType->size() * gep->constantArrayIndex();
    for (size_t index: gep->memberIndices()) {
        auto* record = cast<RecordType const*>(currentType);
        offset += record->offsetAt(index);
        currentType = record->elementAt(index);
    }
    return offset;
}

bool Variable::analyzeImpl(GetElementPointer* gep) {
    if (!gep->hasConstantArrayIndex()) {
        return false;
    }
    if (!isa<Phi>(gep->basePointer())) {
        addPointer(gep,
                   getPtrOffset(gep->basePointer()) + computeGepOffset(gep));
    }
    else {
        addPointer(gep, size_t(-1));
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

static void forwardBFS(Function& function,
                       utl::function_view<void(BasicBlock*)> fn) {
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

bool Variable::rewritePhis() {
    if (phis.empty()) {
        return false;
    }
    /// We split critical edges so we can safely copy users of phi instructions
    /// to predecessors of the phis
    splitCriticalEdges(ctx, function);
    utl::small_vector<Instruction*> toErase;
    utl::hashmap<std::pair<BasicBlock*, Value*>, Instruction*> toCopyMap;
    forwardBFS(function, [&](BasicBlock* BB) {
        for (auto& inst: *BB | Filter<Load, Store, GetElementPointer>) {
            auto* phi = getAssocPhi(&inst);
            if (!phi) {
                continue;
            }
            /// We look at all instructions that have an associated phi node.
            /// We make copies of the instructions in each of the predecessor
            /// blocks of the phi
            utl::small_vector<PhiMapping> newPhiArgs;
            for (auto [pred, phiArgument]: phi->arguments()) {
                SC_ASSERT(pred->numSuccessors() == 1,
                          "If our phi block BB has multiple predecessors then "
                          "this is guaranteed because we have split the "
                          "critical edges. However if if have phi nodes with "
                          "one predecessor this might fail. In this case we "
                          "can probably just delete the phi node. This check "
                          "is needed because we don't want to speculatively "
                          "move instructions to places where they otherwise "
                          "would not be executed.");
                auto* copy = copyInstruction(&inst, pred);
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
                        addPointer(gep, *baseOffset + computeGepOffset(gep));
                    }
                }
            }
            /// If the instruction is a load we phi the copied loads together
            /// We also prune a little bit here to avoid adding unused phi nodes
            if (isa<Load>(inst) && inst.isUsed()) {
                BasicBlockBuilder builder(ctx, phi->parent());
                auto* newPhi =
                    builder.insert<Phi>(phi,
                                        newPhiArgs,
                                        utl::strcat(inst.name(), ".phi"));
                inst.replaceAllUsesWith(newPhi);
            }
            toErase.push_back(&inst);
        }
    });
    for (auto* inst: toErase) {
        forget(inst);
        inst->parent()->erase(inst);
    }
    /// At this stage the phi nodes should only be used by other phi nodes and
    /// we erase all of them
    for (auto* phi: phis) {
        SC_ASSERT(ranges::all_of(phi->users(),
                                 [](auto* user) { return isa<Phi>(user); }),
                  "All users of the phis must be other phis at this point");
        phi->parent()->erase(phi);
    }
    phis.clear();
    return true;
}

Instruction* Variable::copyInstruction(Instruction* inst, BasicBlock* dest) {
    auto* copy = clone(ctx, inst).release();
    dest->insert(dest->terminator(), copy);
    return copy;
}

static utl::small_vector<Slice> slicesInRange(size_t begin,
                                              size_t end,
                                              std::span<Slice const> slices) {
    return ranges::make_subrange(ranges::lower_bound(slices,
                                                     begin,
                                                     ranges::less{},
                                                     &Slice::begin),
                                 ranges::upper_bound(slices,
                                                     end,
                                                     ranges::less{},
                                                     &Slice::end)) |
           ranges::views::transform([&](Slice slice) {
               return Slice(slice.begin() - begin,
                            slice.end() - begin,
                            slice.newAlloca());
           }) |
           ToSmallVector<>;
}

/// Uniform interface to get the associated type of load and store instructions
Type const* getLSType(Instruction const* inst) {
    // clang-format off
    return SC_MATCH (*inst) {
        [](Load const& load) { return load.type(); },
        [](Store const& store) { return store.value()->type(); },
        [](Instruction const& inst) -> Type const* { SC_UNREACHABLE(); },
    }; // clang-format on
}

/// Uniform interface to get the associated pointer of load and store
/// instructions
Value const* getLSPointer(Instruction const* inst) {
    // clang-format off
    return SC_MATCH (*inst) {
        [](Load const& load) { return load.address(); },
        [](Store const& store) { return store.address(); },
        [](Instruction const& inst) -> Value const* { SC_UNREACHABLE(); },
    }; // clang-format on
}

/// \overload
Value* getLSPointer(Instruction* inst) {
    return const_cast<Value*>(getLSPointer(&std::as_const(*inst)));
}

bool Variable::computeSlices() {
    utl::hashset<size_t> set;
    /// We insert all the slice points at the positions that we directly load
    /// from and store to
    for (auto* inst: loadsAndStores) {
        auto [begin, end] = getRange(inst);
        set.insert(begin);
        set.insert(end);
    }
    /// Then we insert all the slice points at "critical positions".
    /// If we slice at a certain member offset, we also need to slice the alloca
    /// at all offsets of siblings in the member tree of that node to be able to
    /// store all siblings
    for (auto* inst: loadsAndStores) {
        auto& tree = sroa.getMemberTree(getLSType(inst));
        size_t const offset = getPtrOffset(getLSPointer(inst));
        utl::small_vector<MemberTree::Node const*> criticalSlicePoints;
        tree.root()->preorderDFS([&](MemberTree::Node const* node) {
            if (!node->parent()) {
                return;
            }
            if (node->begin() != node->parent()->begin() &&
                set.contains(offset + node->begin()))
            {
                criticalSlicePoints.push_back(node);
            }
            if (node->end() != node->parent()->end() &&
                set.contains(offset + node->end()))
            {
                criticalSlicePoints.push_back(node);
            }
        });
        for (auto* node: criticalSlicePoints) {
            auto* parent = node->parent();
            SC_ASSERT(parent,
                      "node should not be in the list if it does not have a "
                      "parent, see check above");
            for (auto* node: parent->children()) {
                set.insert(offset + node->begin());
                set.insert(offset + node->end());
            }
        }
    }
    auto sortedSet = set | ToSmallVector<>;
    ranges::sort(sortedSet);
    auto primSlices =
        ranges::views::zip(sortedSet, sortedSet | ranges::views::drop(1));
    utl::small_vector<Slice> slices;
    slices.reserve(sortedSet.size() - 1);
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
    for (auto* inst: loadsAndStores) {
        auto [begin, end] = getRange(inst);
        instToSlicesMap[inst] = slicesInRange(begin, end, slices);
    }
    return modified;
}

std::pair<size_t, size_t> Variable::getRange(Instruction const* inst) {
    size_t offset = getPtrOffset(getLSPointer(inst));
    return std::pair{ offset, offset + getLSType(inst)->size() };
}

bool Variable::replaceBySlices() {
    bool modified = false;
    for (auto* inst: loadsAndStores) {
        modified |=
            visit(*inst, [&](auto& inst) { return replaceBySlices(&inst); });
    }
    return modified;
}

static void memTreePostorder(
    MemberTree const& tree,
    std::span<Slice const> slices,
    utl::function_view<void(MemberTree::Node const*,
                            std::span<Slice const>,
                            std::span<size_t const>)> fn) {
    utl::small_vector<size_t> indices;
    auto sliceItr = slices.begin();
    auto impl =
        [&](auto& impl, auto& sliceItr, MemberTree::Node const* node) -> bool {
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
    auto slices = getSlices(load);
    bool modified = false;
    Value* aggregate = ctx.undef(load->type());
    memTreePostorder(tree,
                     slices,
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
                auto* newLoad = builder.insert<Load>(load,
                                                     slice.newAlloca(),
                                                     node->type(),
                                                     std::string(load->name()));
                aggregate = builder.insert<InsertValue>(load,
                                                        aggregate,
                                                        newLoad,
                                                        indices,
                                                        "sroa.insert");
                modified = true;
            }
            break;
        }
        default:
            SC_UNIMPLEMENTED();
            break;
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
    auto slices = getSlices(store);
    bool modified = false;
    memTreePostorder(tree,
                     slices,
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
                auto* extr = builder.insert<ExtractValue>(store,
                                                          store->value(),
                                                          indices,
                                                          "sroa.extract");
                builder.insert<Store>(store, slice.newAlloca(), extr);
                modified = true;
            }
            break;
        }
        default:
            SC_UNIMPLEMENTED();
            break;
        }
    });
    if (modified) {
        store->parent()->erase(store);
    }
    return modified;
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
