#include "Opt/Passes.h"

#include <optional>
#include <queue>
#include <set>
#include <span>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <utl/function_view.hpp>
#include <utl/hashtable.hpp>
#include <utl/strcat.hpp>
#include <utl/vector.hpp>

#include "Common/Allocator.h"
#include "Common/Ranges.h"
#include "IR/CFG.h"
#include "IR/Clone.h"
#include "IR/Context.h"
#include "IR/Dominance.h"
#include "IR/Loop.h"
#include "IR/Type.h"
#include "IR/Validate.h"
#include "Opt/Common.h"
#include "Opt/MemberTree.h"
#include "Opt/PassRegistry.h"

#include "IR/Print.h"
#include <iostream>

using namespace scatha;
using namespace ir;
using namespace opt;

SC_REGISTER_PASS(opt::sroa2, "sroa2");

namespace {

struct Slice {
    size_t begin, end;
    Alloca* newAlloca;
};

struct Variable {
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

    /// Maps loads, stores and geps to the phi node that transitively get their
    /// pointer from
    utl::hashmap<Instruction*, Phi*> assocPhis;

    /// Maps all pointer instructions to their offset into the alloca region
    utl::hashmap<Instruction const*, size_t> ptrToOffsetMap;

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

    bool addPointer(Instruction* ptr, size_t offset) {
        return ptrToOffsetMap.insert({ ptr, offset }).second;
    }

    /// Maps load and store instructions to a range of slices that it should
    /// load from or store to
    utl::hashmap<Instruction const*, utl::small_vector<Slice>> instToSlicesMap;

    std::span<Slice const> getSlices(Instruction const* inst) const {
        auto itr = instToSlicesMap.find(inst);
        SC_ASSERT(itr != instToSlicesMap.end(), "Not found");
        return itr->second;
    }

    Variable(Context& ctx, Function& function, Alloca* baseAlloca):
        ctx(ctx),
        function(function),
        LNF(function.getOrComputeLNF()),
        baseAlloca(baseAlloca) {}

    Phi* getAssocPhi(Instruction* inst) const {
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
    void rewritePhis();
    Instruction* copyInstruction(Instruction* inst, BasicBlock* dest);

    ///
    void computeSlices();
    std::pair<size_t, size_t> getRange(Instruction const* inst);

    ///
    void replaceBySlices();
    void replaceBySlices(Load const* load);
    void replaceBySlices(Store const* store);
    void replaceBySlices(Instruction const* inst) { SC_UNREACHABLE(); }
};

} // namespace

bool opt::sroa2(Context& ctx, Function& function) {
    auto worklist =
        function.entry() | Filter<Alloca> | TakeAddress | ToSmallVector<>;
    for (auto* baseAlloca: worklist) {
        Variable var(ctx, function, baseAlloca);
        if (!var.run()) {
            continue;
        }
    }
    return false;
}

bool Variable::run() {
    if (!analyze(baseAlloca)) {
        return false;
    }
    rewritePhis();
    computeSlices();
    replaceBySlices();
    return true;
}

bool Variable::analyze(Instruction* inst) {
    return visit(*inst, [&](auto& inst) { return analyzeImpl(&inst); });
}

bool Variable::analyzeUsers(Instruction* inst) {
    return ranges::all_of(inst->users(), [&](auto* user) {
        if (Phi* phi = nullptr;
            (phi = dyncast<Phi*>(inst)) || (phi = getAssocPhi(inst)))
        {
            assocPhis[user] = phi;
        }
        return analyze(user);
    });
}

bool Variable::analyzeImpl(Instruction* inst) { return false; }

bool Variable::analyzeImpl(Alloca* allocaInst) {
    SC_ASSERT(allocaInst == baseAlloca, "");
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
        // clang-format off
        SC_MATCH (*currentType) {
            [&](StructType const& type) {
                offset += type.memberOffsetAt(index);
                currentType = type.memberAt(index);
            },
            [&](ArrayType const& type) {
                SC_ASSERT(index < type.count(), "Index out of bounds");
                offset += index * type.elementType()->size();
                currentType = type.elementType();
            },
            [](Type const&) { SC_UNREACHABLE(); }
        }; // clang-format on
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

static void reverseBFS(Function& function,
                       utl::function_view<void(BasicBlock*)> fn) {
    auto visited = function | TakeAddress | ranges::views::filter([](auto* BB) {
                       return isa<Return>(BB->terminator());
                   }) |
                   ranges::to<utl::hashset<BasicBlock*>>;
    std::queue<BasicBlock*> queue;
    for (auto* BB: visited) {
        queue.push(BB);
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

void Variable::rewritePhis() {
    if (phis.empty()) {
        return;
    }
    /// We split critical edges so we can safely copy users of phi instructions
    /// to predecessors of the phis
    splitCriticalEdges(ctx, function);
    utl::small_vector<Instruction*> toErase;
    utl::hashmap<std::pair<BasicBlock*, Instruction*>, Instruction*> toCopyMap;
    reverseBFS(function, [&](BasicBlock* BB) {
        for (auto& inst: *BB | Filter<Load, Store, GetElementPointer>) {
            auto* phi = getAssocPhi(&inst);
            if (!phi) {
                continue;
            }
            /// We look at all instructions that have an associated phi node.
            /// We make copies of the instructions in each of the predecessor
            /// blocks of the phi
            utl::small_vector<PhiMapping> newPhiArgs;
            for (auto [pred, value]: phi->arguments()) {
                auto* copy = copyInstruction(&inst, pred);
                toCopyMap[{ pred, &inst }] = copy;
                if (tryGetPtrOffset(value)) {
                    memorize(copy);
                }
                for (auto [index, operand]:
                     copy->operands() | ranges::views::enumerate)
                {
                    if (operand == phi) {
                        copy->setOperand(index, value);
                        continue;
                    }
                    auto itr = toCopyMap.find(
                        { pred, dyncast<Instruction*>(operand) });
                    if (itr != toCopyMap.end()) {
                        copy->setOperand(index, itr->second);
                    }
                }
                if (auto* gep = dyncast<GetElementPointer*>(copy)) {
                    if (auto baseOffset = tryGetPtrOffset(gep->basePointer())) {
                        addPointer(gep, *baseOffset + computeGepOffset(gep));
                    }
                }
                newPhiArgs.push_back({ pred, copy });
                if (auto* assocPhi = getAssocPhi(dyncast<Instruction*>(value)))
                {
                    assocPhis[copy] = assocPhi;
                }
                if (auto* assocPhi = dyncast<Phi*>(value)) {
                    assocPhis[copy] = assocPhi;
                }
            }
            /// If the instruction is a load we phi the copied loads together
            /// We also prune a little bit here to avoid adding unused phi nodes
            if (isa<Load>(inst) && inst.isUsed()) {
                auto* newPhi =
                    new Phi(newPhiArgs, utl::strcat(inst.name(), ".phi"));
                phi->parent()->insert(phi, newPhi);
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
           ranges::views::transform(
               [&](Slice slice) {
        return Slice{ slice.begin - begin, slice.end - begin, slice.newAlloca };
           }) |
        ToSmallVector<>;
}

void Variable::computeSlices() {
    std::set<size_t> set;
    for (auto* inst: loadsAndStores) {
        auto [begin, end] = getRange(inst);
        set.insert(begin);
        set.insert(end);
    }
    auto slices = ranges::views::zip(set, set | ranges::views::drop(1)) |
                  ranges::views::transform([&](auto p) {
                      auto [begin, end] = p;
                      auto* newAlloca =
                          new Alloca(ctx,
                                     ctx.intConstant(end - begin, 32),
                                     ctx.intType(8),
                                     utl::strcat(baseAlloca->name(), ".slice"));
                      function.entry().insert(baseAlloca, newAlloca);
                      return Slice{ begin, end, newAlloca };
                  }) |
                  ToSmallVector<>;
    for (auto* inst: loadsAndStores) {
        auto [begin, end] = getRange(inst);
        instToSlicesMap[inst] = slicesInRange(begin, end, slices);
    }
}

std::pair<size_t, size_t> Variable::getRange(Instruction const* inst) {
    // clang-format off
    return SC_MATCH (*inst) {
        [&](Load const& load) {
            size_t offset = getPtrOffset(load.address());
            return std::pair{ offset, offset + load.type()->size() };
        },
        [&](Store const& store) {
            size_t offset = getPtrOffset(store.address());
            return std::pair{ offset, offset + store.value()->type()->size() };
        },
        [&](Instruction const& inst) -> std::pair<size_t, size_t> {
            SC_UNREACHABLE();
        }
    }; // clang-format on
}

void Variable::replaceBySlices() {
    for (auto* inst: loadsAndStores) {
        visit(*inst, [&](auto& inst) { replaceBySlices(&inst); });
    }
}

static void memTreePostorder(
    MemberTree const& tree,
    std::span<Slice const> slices,
    utl::function_view<void(MemberTree::Node const*,
                            std::span<Slice const>,
                            std::span<size_t const>)> fn) {
    utl::small_vector<size_t> indices;
    auto itr = slices.begin();
    auto impl = [&](auto& impl, MemberTree::Node const* node) -> void {
        for (auto* child: node->children()) {
            indices.push_back(child->index());
            impl(impl, child);
            indices.pop_back();
        }
        auto begin = itr;
        while (itr != slices.end() && itr->end <= node->end()) {
            ++itr;
        }
        auto end = itr;
        utl::small_vector<Slice> ourSlices(begin, end);
        fn(node, ourSlices, indices);
    };
    impl(impl, tree.root());
}

void Variable::replaceBySlices(Load const* load) {
    auto slices = getSlices(load);
    auto memberTree = MemberTree::compute(load->type());
    print(memberTree);
    memTreePostorder(memberTree,
                     slices,
                     [&](MemberTree::Node const* node,
                         std::span<Slice const> slices,
                         std::span<size_t const> indices) {
        std::cout << "  " << *node->type() << ": ";
        for (auto slice: slices) {
            std::cout << "[" << slice.begin << ", " << slice.end << "), ";
        }
        std::cout << std::endl;
        std::cout << "    ";
        for (size_t index: indices) {
            std::cout << "." << index;
        }
        std::cout << std::endl;
    });
}

void Variable::replaceBySlices(Store const* store) {}
