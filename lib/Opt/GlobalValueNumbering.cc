#include "Opt/Passes.h"

#include <array>
#include <bit>
#include <queue>

#include <iostream>

#include <range/v3/algorithm.hpp>
#include <utl/function_view.hpp>
#include <utl/graph.hpp>
#include <utl/hash.hpp>
#include <utl/hashtable.hpp>
#include <utl/ilist.hpp>
#include <utl/stack.hpp>

#include "Common/Ranges.h"
#include "IR/CFG.h"
#include "IR/Clone.h"
#include "IR/Dominance.h"
#include "IR/Loop.h"
#include "IR/Validate.h"
#include "Opt/Common.h"
#include "Opt/PassRegistry.h"

using namespace scatha;
using namespace opt;
using namespace ir;

SC_REGISTER_PASS(opt::globalValueNumbering, "gvn");

namespace {

/// Structure used to identify identical computations
struct Computation {
    Computation(Instruction* inst): inst(inst) {}

    /// The associated instruction
    Instruction* instruction() const { return inst; }

    /// \Returns `true` iff  \p *this and \p RHS compute the same value
    bool operator==(Computation const& RHS) const {
        return equal(inst, inst->operands(), RHS.inst, RHS.inst->operands());
    }

    /// A hash value based on the kind of computation and the addresses of the
    /// operands
    size_t hashValue() const { return hash(inst, inst->operands()); }

    /// Implementation of `operator==`. Exposed here for use in 'question
    /// propagation' in loop header nodes
    static bool equal(Instruction const* A,
                      std::span<Value const* const> AOps,
                      Instruction const* B,
                      std::span<Value const* const> BOps) {
        if (A->nodeType() != B->nodeType()) {
            return false;
        }
        // clang-format off
        bool const compEqual = utl::visit(*A, utl::overload{
            [&](ArithmeticInst const& inst) {
                return inst.operation() ==
                           cast<ArithmeticInst const*>(B)->operation();
            },
            [&](UnaryArithmeticInst const& inst) {
                return inst.operation() ==
                           cast<UnaryArithmeticInst const*>(B)->operation();
            },
            [&](CompareInst const& inst) {
                return inst.operation() ==
                           cast<CompareInst const*>(B)->operation();
            },
            [&](Call const& inst) -> bool {
                SC_UNIMPLEMENTED();
            },
            [&](ExtractValue const& inst) {
                return ranges::equal(
                           inst.memberIndices(),
                           cast<ExtractValue const*>(B)->memberIndices());
            },
            [&](InsertValue const& inst) {
                return ranges::equal(
                           inst.memberIndices(),
                           cast<InsertValue const*>(B)->memberIndices());
            },
            [&](GetElementPointer const& inst) {
                return ranges::equal(
                           inst.memberIndices(),
                           cast<GetElementPointer const*>(B)->memberIndices());
            },
            [&](ConversionInst const& inst) {
                return inst.conversion() ==
                           cast<ConversionInst const*>(B)->conversion();
            },
            [&](Select const& inst) {
                return true;
            },
            [&](Instruction const& inst) -> bool {
                SC_UNREACHABLE();
            }
        }); // clang-format on
        if (!compEqual) {
            return false;
        }
        return ranges::equal(AOps, BOps);
    }

    static size_t hash(Instruction const* inst,
                       std::span<Value const* const> ops) {
        size_t seed = 0;
        utl::hash_combine_seed(seed, inst->nodeType());

        // clang-format off
        utl::visit(*inst, utl::overload{
            [&](ArithmeticInst const& inst) {
                utl::hash_combine_seed(seed, inst.operation());
            },
            [&](UnaryArithmeticInst const& inst) {
                utl::hash_combine_seed(seed, inst.operation());
            },
            [&](CompareInst const& inst) {
                utl::hash_combine_seed(seed, inst.operation());
            },
            [&](Call const& inst) {
                SC_UNIMPLEMENTED();
            },
            [&](ExtractValue const& inst) {
                for (size_t index: inst.memberIndices()) {
                    utl::hash_combine_seed(seed, index);
                }
            },
            [&](InsertValue const& inst) {
                for (size_t index: inst.memberIndices()) {
                    utl::hash_combine_seed(seed, index);
                }
            },
            [&](GetElementPointer const& inst) {
                for (size_t index: inst.memberIndices()) {
                    utl::hash_combine_seed(seed, index);
                }
            },
            [&](ConversionInst const& inst) {
                utl::hash_combine_seed(seed, inst.conversion());
            },
            [&](Select const& inst) {},
            [&](Instruction const& inst) {
                SC_UNREACHABLE();
            }
        }); // clang-format on
        for (auto* operand: ops) {
            utl::hash_combine_seed(seed, operand);
        }
        return seed;
    }

private:
    Instruction* inst;
};

} // namespace

template <>
struct std::hash<Computation> {
    size_t operator()(Computation expr) const { return expr.hashValue(); }
};

/// \returns `true` for all instructions ignored by the algorithm
static bool isIgnored(Instruction const* inst) {
    // clang-format off
    return visit(*inst, utl::overload{
        [](Phi const&)            { return true; },
        [](TerminatorInst const&) { return true; },
        [](Alloca const&)         { return true; },
        [](Load const&)           { return true; },
        [](Store const&)          { return true; },
        [](Call const&)           { return true; },
        [](Instruction const&)    { return false; }
    }); // clang-format on
}

namespace {

/// A local computation table exists for every block and holds the computations
/// of that basic block in a way that we can conveniently
/// - loop over all computations
/// - loop over all computations of a given rank
/// - identify all identical computations for a given computation
class LocalComputationTable {
public:
    /// Insert a computation into the table
    /// \Returns `nullptr` on success or otherwise the existing instruction that
    /// computes the same value.
    [[nodiscard]] Instruction* insertOrExisting(size_t rank,
                                                Instruction* inst) {
        _maxRank = std::max(_maxRank, rank);
        auto& computations = rankMap[rank];
        auto [itr, success] = computations.insert(Computation(inst));
        if (success) {
            return nullptr;
        }
        return itr->instruction();
    }

    /// View over all computations
    auto instructions() const {
        return rankMap | ranges::views::transform([](auto& p) -> auto& {
                   return p.second;
               }) |
               ranges::views::join | CompToInst;
    }

    /// View over all computations of a given rank
    auto instructions(size_t rank) { return rankMap[rank] | CompToInst; }

    /// The maximum rank of computations in this LCT
    size_t maxRank() const { return _maxRank; }

private:
    static constexpr auto CompToInst = ranges::views::transform(
        [](Computation comp) { return comp.instruction(); });

    utl::hashmap<size_t, utl::hashset<Computation>> rankMap;
    size_t _maxRank = 0;
};

/// A movable computation table exists for every forward edge in the CFG. It
/// contains all computations that available for movement from the successor to
/// the predecessor. Each entry in the MCT holds a computation and a pointer to
/// the corresponding computation in the LCT of the successor block.
class MovableComputationTable {
    auto allEntries() {
        return _entries | ranges::views::transform([](auto& p) -> auto& {
                   return p.second;
               }) |
               ranges::views::join;
    }

public:
    class Entry: public utl::ilist_node<Entry> {
    public:
        Entry(UniquePtr<Instruction> copy, Instruction* original):
            _copy(std::move(copy)), _originals({ original }) {}

        /// Local copy of the instruction in the MCT
        Instruction* copy() const { return _copy.get(); }

        /// Caller takes ownership of the copy
        Instruction* takeCopy() { return _copy.release(); }

        /// Pointer to the original instruction in the LCT of the successor
        std::span<Instruction* const> originals() const { return _originals; }

        /// Rank of this entry
        size_t rank() const { return _rank; }

    private:
        friend class MovableComputationTable;

        UniquePtr<Instruction> _copy;
        utl::small_vector<Instruction*> _originals;
        utl::ilist<Entry>* list = nullptr;
        size_t _rank = ~0ull;
    };

    /// Insert an entry into the MCT
    void insert(size_t rank,
                UniquePtr<Instruction> copy,
                Instruction* original) {
        auto itr = compMap.find(copy.get());
        if (itr != compMap.end()) {
            auto* existingEntry = itr->second;
            existingEntry->_originals.push_back(original);
            return;
        }
        auto& list = _entries[rank];
        Entry entry(std::move(copy), original);
        entry.list = &list;
        entry._rank = rank;
        auto* instCopy = entry.copy();
        list.push_back(std::move(entry));
        auto* entryPtr = &list.back();
        compMap.insert({ instCopy, entryPtr });
    }

    /// \Returns `true` iff this MCT has any computations that are equal to \p
    /// inst
    bool hasComputationEqualTo(Instruction* inst) const {
        return compMap.contains(Computation(inst));
    }

    /// Erase all computations equal to \p inst
    void eraseComputationEqualTo(Instruction* inst) {
        auto itr = compMap.find(Computation(inst));
        if (itr == compMap.end()) {
            return;
        }
        auto* entry = itr->second;
        compMap.erase(itr);
        entry->list->erase(entry);
    }

    auto computationEqualTo(Instruction* inst) const {
        auto itr = compMap.find(Computation(inst));
        SC_ASSERT(itr != compMap.end(), "");
        return itr->second;
    }

    /// Erase all computations from the table
    void clear() {
        _entries.clear();
        compMap.clear();
    }

    /// Erase all computations from the table with a given rank
    void erase(size_t rank) {
        for (auto& entry: _entries[rank]) {
            compMap.erase(entry.copy());
        }
        _entries.erase(rank);
    }

    /// Erase a single entry from the table
    void erase(Computation comp, Entry* entry) {
        compMap.erase(comp);
        entry->list->erase(entry);
    }

    /// Returns a view over all computations of a given rank
    auto& entries(size_t rank) { return _entries[rank]; }

    /// Iterator to the first computation of the table.
    /// \Note: `[begin(), end())` spans all computations in the table regardless
    /// of rank.
    auto begin() { return allEntries().begin(); }

    /// Iterator past the end.
    auto end() { return allEntries().end(); }

private:
    utl::node_hashmap<size_t, utl::ilist<Entry>> _entries;
    utl::hashmap<Computation, Entry*> compMap;
};

struct Loop {
    BasicBlock* landingPad;
    BasicBlock* header;

    /// All nodes in the loop
    utl::hashset<BasicBlock*> loopNodes = {};

    /// All nodes that the loop may exit to
    utl::hashset<BasicBlock*> exitNodes = {};
};

/// Represents an edge in the CFG
using Edge = std::pair<BasicBlock*, BasicBlock*>;

template <typename T>
using RankMap = utl::hashmap<T*, size_t>;

struct GVNContext {
    Context& ctx;
    Function& function;

    bool modified = false;
    utl::hashset<BasicBlock*> edgeSplitBlocks;
    utl::hashset<BasicBlock*> loopHeaders;
    utl::hashset<BasicBlock*> landingPads;
    /// Maps headers to loops
    utl::hashmap<BasicBlock*, Loop> loops;
    /// Virtual back edges are edges from loop exit nodes to their corresponding
    /// landing pads
    utl::hashmap<BasicBlock*, utl::small_vector<BasicBlock*>>
        virtualPredecessors;
    utl::small_vector<BasicBlock*> topsortOrder;
    size_t maxRank = 0;
    RankMap<Value> globalRanks;
    utl::hashset<Instruction*> redundant;
    utl::hashmap<BasicBlock*, RankMap<Instruction>> localRanks;
    utl::hashmap<std::pair<BasicBlock*, size_t>, Instruction*> insertPoints;
    utl::hashmap<BasicBlock*, LocalComputationTable> LCTs;
    utl::hashmap<Edge, MovableComputationTable> MCTs;

    GVNContext(Context& context, Function& function):
        ctx(context), function(function) {}

    bool run();

    void gatherLoops();
    void splitCriticalEdges();
    void joinSplitEdges();
    void computeTopsortOrder();
    void assignRanks();
    /// Remove all instructions that have become redundant
    void clean();

    /// The main algorithm
    void processGlobally();
    void processHeader(size_t rank, BasicBlock* BB, LocalComputationTable&);
    /// \Returns `true` if the computation \p inst can be moved out of a loop
    /// header
    bool isHeaderMovable(Instruction* inst,
                         BasicBlock* header,
                         BasicBlock* landingPad);

    void processLandingPad(size_t rank, BasicBlock*, LocalComputationTable&);
    void processOther(size_t rank, BasicBlock*, LocalComputationTable&);
    void moveIn(size_t rank, BasicBlock*, LocalComputationTable&);
    void moveInImpl(size_t rank,
                    BasicBlock*,
                    LocalComputationTable&,
                    std::span<BasicBlock* const> succs,
                    utl::function_view<bool(Instruction const*)> condition);
    void moveOut(size_t rank, BasicBlock*, LocalComputationTable&);

    Instruction* insertPointForRank(BasicBlock* BB, size_t rank) {
        auto itr = insertPoints.find({ BB, rank });
        SC_ASSERT(itr != insertPoints.end(),
                  "We added insertion points for all ranks and blocks");
        return itr->second;
    }

    void buildLCTs();
    size_t computeRank(Instruction* inst);
    size_t getAvailRank(Value* value);
    LocalComputationTable buildLCT(BasicBlock* BB);
};

} // namespace

bool opt::globalValueNumbering(Context& ctx, Function& function) {
    bool result = false;
    result |= rotateLoops(ctx, function);
    /// We limit the scope of the `GVNContext`. It may still hold cloned
    /// instructions that must be deleted before `assertInvariants` runs.
    result |= GVNContext(ctx, function).run();
    assertInvariants(ctx, function);
    return result;
}

bool GVNContext::run() {
    splitCriticalEdges();
    gatherLoops();
    computeTopsortOrder();
    assignRanks();
    processGlobally();
    clean();
    joinSplitEdges();
    /// We invalidate the CFG info unconditionally because we did modify the
    /// CFG, compute CFG info and then potentially undid all changes to the CFG.
    /// But because we recomputed the info in between we have to invalidate
    /// again.
    function.invalidateCFGInfo();
    return modified;
}

void GVNContext::gatherLoops() {
    auto& LNF = function.getOrComputeLNF();
    LNF.traversePreorder([&](LNFNode const* node) {
        if (!node->isProperLoop()) {
            return;
        }
        auto* header = node->basicBlock();

        /// Find a canditate for a landing pad
        auto isLPCandidate = [&](BasicBlock* pred) {
            return !ranges::contains(node->children(), LNF[pred]) &&
                   pred != header;
        };
        SC_ASSERT(ranges::count_if(header->predecessors(), isLPCandidate) == 1,
                  "We can only have one landing pad per loop header");
        auto* candidate =
            *ranges::find_if(header->predecessors(), isLPCandidate);
        auto* landingPad = candidate;

        /// If the candidate has multiple successors it is not a landing pad but
        /// a loop guard. We insert a landing pad between the guard and the
        /// header.
        if (candidate->numSuccessors() > 1) {
            auto* guard = candidate;
            landingPad = new BasicBlock(ctx, "loop.landingpad");
            edgeSplitBlocks.insert(landingPad);
            function.insert(guard, landingPad);
            guard->terminator()->updateTarget(header, landingPad);
            landingPad->pushBack(new Goto(ctx, header));
            header->updatePredecessor(guard, landingPad);
        }

        /// Gather the entire loop
        auto& loop = loops[header] = Loop{ landingPad, header };
        for (auto* pred: header->predecessors()) {
            auto dfs = [&](auto& dfs, BasicBlock* BB) {
                if (BB == landingPad) {
                    return;
                }
                if (!loop.loopNodes.insert(BB).second) {
                    return;
                }
                for (auto* pred: BB->predecessors()) {
                    dfs(dfs, pred);
                }
            };
            dfs(dfs, pred);
        }

        /// Gather the exit nodes and virtual back edges
        for (auto* BB: loop.loopNodes) {
            for (auto* succ: BB->successors()) {
                if (!loop.loopNodes.contains(succ)) {
                    loop.exitNodes.insert(succ);
                    virtualPredecessors[succ].push_back(landingPad);
                }
            }
        }

        loopHeaders.insert(header);
        landingPads.insert(landingPad);
    });
}

/// We do not use the `splitcriticaledges` pass here. We we need to erase the
/// inserted basic blocks if we did not move any code into them. Otherwise,
/// running this pass and `simplyfygfc` repeatedly will result in an infite
/// modification cycle, because `simplyfygfc` erases the added basic blocks.
void GVNContext::splitCriticalEdges() {
    struct DFS {
        Context& ctx;
        utl::hashset<BasicBlock*>& insertedBlocks;
        utl::hashset<BasicBlock*> visited = {};

        void search(BasicBlock* BB) {
            if (!visited.insert(BB).second) {
                return;
            }
            for (auto* succ: BB->successors()) {
                if (isCriticalEdge(BB, succ)) {
                    insertedBlocks.insert(splitEdge(ctx, BB, succ));
                }
                search(succ);
            }
        }
    };
    DFS{ ctx, edgeSplitBlocks }.search(&function.entry());
    if (!edgeSplitBlocks.empty()) {
        function.invalidateCFGInfo();
    }
}

static void eraseBlock(BasicBlock* BB) {
    auto* function = BB->parent();
    auto* pred = BB->singlePredecessor();
    auto* succ = BB->singleSuccessor();
    pred->terminator()->updateTarget(BB, succ);
    succ->updatePredecessor(BB, pred);
    function->erase(BB);
}

void GVNContext::joinSplitEdges() {
    auto blocks = edgeSplitBlocks | ranges::to<utl::small_vector<BasicBlock*>>;
    for (auto* BB: blocks) {
        if (BB->emptyExceptTerminator() && BB->hasSinglePredecessor() &&
            BB->hasSingleSuccessor())
        {
            edgeSplitBlocks.erase(BB);
            eraseBlock(BB);
        }
    }
    modified |= !edgeSplitBlocks.empty();
}

void GVNContext::computeTopsortOrder() {
    struct DFS {
        utl::hashset<BasicBlock*> visited;
        utl::hashset<BasicBlock*> visitedCurrentDescend;
        utl::hashset<std::pair<BasicBlock*, BasicBlock*>> backEdges;

        void search(BasicBlock* BB) {
            visited.insert(BB);
            for (auto* succ: BB->successors()) {
                if (visitedCurrentDescend.contains(succ)) {
                    backEdges.insert({ BB, succ });
                    continue;
                }
                if (visited.contains(succ)) {
                    continue;
                }
                visitedCurrentDescend.insert(succ);
                search(succ);
                visitedCurrentDescend.erase(succ);
            }
        }
    };

    DFS dfs;
    dfs.search(&function.entry());
    topsortOrder = function |
                   ranges::views::transform([](auto& BB) { return &BB; }) |
                   ranges::to<utl::small_vector<BasicBlock*>>;
    auto forwardEdges = [&](BasicBlock* BB) {
        return BB->successors() |
               ranges::views::filter([&dfs, BB](BasicBlock* succ) {
                   return !dfs.backEdges.contains({ BB, succ });
               });
    };
    utl::topsort(topsortOrder.begin(), topsortOrder.end(), forwardEdges);
}

void GVNContext::assignRanks() {
    utl::hashmap<Instruction*, size_t> instructionOrder;
    for (auto* BB: topsortOrder | ranges::views::reverse) {
        for (auto&& [index, inst]: *BB | ranges::views::enumerate) {
            instructionOrder[&inst] = index;
            if (isa<TerminatorInst>(inst)) {
                continue;
            }
            size_t rank = computeRank(&inst);
            globalRanks[&inst] = rank;
            localRanks[BB][&inst] = rank;
            insertPoints[{ BB, rank }] = inst.next();
            maxRank = std::max(maxRank, rank);
        }
    }
    /// Here we ensure that all blocks and ranks have an insertion point and
    /// that insertion points of higher rank never precede insertion points of
    /// lower rank. This may happen if in the original code higher rank
    /// instructions precede lower rank instructions.
    for (auto& BB: function) {
        insertPoints.insert({ { &BB, 0 }, BB.terminator() });
        for (size_t rank = 1; rank <= maxRank; ++rank) {
            auto* insertPoint = insertPoints[{ &BB, rank }];
            auto* prevInsertPoint = insertPoints[{ &BB, rank - 1 }];
            if (!insertPoint) {
                insertPoint = insertPoints[{ &BB, rank }] = BB.terminator();
            }
            if (!prevInsertPoint) {
                prevInsertPoint = insertPoints[{ &BB, rank - 1 }] =
                    BB.terminator();
            }
            if (instructionOrder[insertPoint] <
                instructionOrder[prevInsertPoint])
            {
                insertPoints[{ &BB, rank }] = prevInsertPoint;
            }
        }
    }
}

size_t GVNContext::computeRank(Instruction* inst) {
    size_t rank = ranges::max(inst->operands() |
                              ranges::views::transform([&](Value* value) {
                                  return getAvailRank(value);
                              }));
    if (!isa<Phi>(inst)) {
        ++rank;
    }
    return rank;
}

size_t GVNContext::getAvailRank(Value* value) { return globalRanks[value]; }

LocalComputationTable GVNContext::buildLCT(BasicBlock* BB) {
    auto& ranks = localRanks[BB];
    LocalComputationTable result;
    for (auto [inst, rank]: ranks) {
        if (isIgnored(inst)) {
            continue;
        }
        if (auto* existing = result.insertOrExisting(rank, inst)) {
            replaceValue(inst, existing);
            redundant.insert(inst);
            modified = true;
        }
    }
    return result;
}

void GVNContext::clean() {
    for (auto* inst: redundant) {
        SC_ASSERT(!inst->isUsed(), "");
        inst->parent()->erase(inst);
    }
}

void GVNContext::processGlobally() {
    for (size_t rank = 0; rank <= maxRank; ++rank) {
        for (auto* BB: topsortOrder) {
            auto& LCT = LCTs[BB];
            if (rank == 0) {
                LCT = buildLCT(BB);
            }
            if (loopHeaders.contains(BB)) {
                processHeader(rank, BB, LCT);
            }
            else if (landingPads.contains(BB)) {
                processLandingPad(rank, BB, LCT);
            }
            else {
                processOther(rank, BB, LCT);
            }
        }
    }
}

static auto isMoveable = [](Instruction* inst) {
    if (isIgnored(inst)) {
        return false;
    }
    for (auto* operand: inst->operands() | Filter<Instruction>) {
        if (isa<Phi>(operand)) {
            continue;
        }
        if (operand->parent() == inst->parent()) {
            return false;
        }
    }
    return true;
};

static UniquePtr<Instruction> copyAndPhiRename(Context& ctx,
                                               Instruction* inst,
                                               BasicBlock* pred) {
    auto copy = ir::clone(ctx, inst);
    for (auto [index, operand]: inst->operands() | ranges::views::enumerate) {
        auto* phi = dyncast<Phi*>(operand);
        if (phi && phi->parent() == inst->parent()) {
            copy->updateOperand(phi, phi->operandOf(pred));
        }
    }
    return copy;
}

void GVNContext::processHeader(size_t rank,
                               BasicBlock* header,
                               LocalComputationTable& LCT) {
    moveIn(rank, header, LCT);
    /// Identify candidates for movement to our predecessor
    auto* landingPad = loops[header].landingPad;
    auto movable = LCT.instructions(rank) |
                   ranges::views::filter([&](auto* inst) {
                       return isHeaderMovable(inst, header, landingPad);
                   }) |
                   ranges::to<utl::small_vector<Instruction*>>;
    auto& MCT = MCTs[{ landingPad, header }];
    for (auto* inst: movable) {
        MCT.insert(rank, copyAndPhiRename(ctx, inst, landingPad), inst);
    }
}

bool GVNContext::isHeaderMovable(Instruction* inst,
                                 BasicBlock* header,
                                 BasicBlock* landingPad) {
    if (isIgnored(inst)) {
        return false;
    }
    return ranges::none_of(inst->operands() | Filter<Instruction>,
                           [&](Instruction* operand) {
        return operand->parent() == header;
    });
#if 0
    struct DFS {
        Instruction* inst;
        BasicBlock* header;
        BasicBlock* landingPad;
        utl::hashmap<BasicBlock*, utl::small_vector<Value*>> operandMap = {};
        bool redundant = true;
        bool visitedHeader = false;

        bool run() {
            for (auto* pred: header->predecessors()) {
                if (pred == landingPad) {
                    continue;
                }
                if (!search(pred, phiRename(header, inst->operands(), pred))) {
                    break;
                }
            }
            return redundant;
        }

        bool search(BasicBlock* BB, utl::small_vector<Value* const> operands) {
            if (BB == header) {
                redundant = false;
                return false;
            }
            auto [itr, success] = operandMap.insert({ BB, operands });
            if (!success) {
                if (ranges::equal(itr->second, operands)) {
                    return true;
                }
                else {
                    redundant = false;
                    return false;
                }
            }
            auto isLocalInst = [&](Value* value) {
                auto* inst = dyncast<Instruction*>(value);
                if (!inst) {
                    return false;
                }
                if (isa<Phi>(inst)) {
                    return false;
                }
                return inst->parent() == BB;
            };
            if (ranges::any_of(operands, isLocalInst)) {
                redundant = false;
                return false;
            }
            /// We should rewrite this at some point to use a constant time
            /// algorithm instead of looping over the entire BB
            for (auto& inst: *BB) {
                if (isa<Phi>(inst) || isa<TerminatorInst>(inst)) {
                    continue;
                }
                if (Computation::equal(&inst,
                                       inst.operands(),
                                       this->inst,
                                       operands))
                {
                    return true;
                }
            }
            for (auto* pred: BB->predecessors()) {
                if (pred == landingPad) {
                    SC_ASSERT(BB == header, "");
                    continue;
                }
                if (!search(pred, phiRename(BB, operands, pred))) {
                    return false;
                }
            }
            return true;
        }

        utl::small_vector<Value*> phiRename(BasicBlock* BB,
                                            std::span<Value* const> operands,
                                            BasicBlock* pred) const {
            return operands | ranges::views::transform([&](Value* operand) {
                       auto* phi = dyncast<Phi*>(operand);
                       if (phi && phi->parent() == BB) {
                           return phi->operandOf(pred);
                       }
                       return operand;
                   }) |
                   ranges::to<utl::small_vector<Value*>>;
        }
    };
    return DFS{ inst, header, landingPad }.run();
#endif
}

void GVNContext::processLandingPad(size_t rank,
                                   BasicBlock* BB,
                                   LocalComputationTable& LCT) {
    auto* header = BB->singleSuccessor();
    auto& loop = loops[header];
    moveInImpl(rank,
               BB,
               LCT,
               BB->successors() | ranges::to<utl::small_vector<BasicBlock*>>,
               [&](Instruction const* inst) {
        return ranges::none_of(inst->operands() | Filter<Instruction>,
                               [&](auto* instOp) {
            return loop.loopNodes.contains(instOp->parent());
        });
    });
    moveOut(rank, BB, LCT);
}

void GVNContext::processOther(size_t rank,
                              BasicBlock* BB,
                              LocalComputationTable& LCT) {
    moveIn(rank, BB, LCT);
    moveOut(rank, BB, LCT);
}

void GVNContext::moveIn(size_t rank,
                        BasicBlock* BB,
                        LocalComputationTable& LCT) {
    moveInImpl(rank,
               BB,
               LCT,
               BB->successors() | ranges::to<utl::small_vector<BasicBlock*>>,
               [](Instruction const* inst) { return true; });
}

void GVNContext::moveInImpl(
    size_t rank,
    BasicBlock* BB,
    LocalComputationTable& LCT,
    std::span<BasicBlock* const> succs,
    utl::function_view<bool(Instruction const*)> condition) {
    auto insertIntoLTCAndBB =
        [&](Instruction* insertPoint, MovableComputationTable::Entry& entry) {
        Instruction* inst = entry.copy();
        if (auto* existing = LCT.insertOrExisting(rank, inst)) {
            inst = existing;
        }
        else {
            globalRanks[inst] = rank;
            BB->insert(insertPoint, entry.takeCopy());
            modified = true;
        }
        for (auto* original: entry.originals()) {
            redundant.insert(original);
            replaceValue(original, inst);
            modified = true;
        }
        return inst;
    };
    /// Move computations from successors into this block
    if (succs.size() == 1) {
        auto& MCT = MCTs[{ BB, succs.front() }];
        auto insertPoint = insertPointForRank(BB, rank);
        for (auto& entry: MCT.entries(rank)) {
            insertIntoLTCAndBB(insertPoint, entry);
        }
        MCT.clear();
        return;
    }
    auto insertPoint = insertPointForRank(BB, rank);
    for (auto* succ: succs) {
        auto& MCT_A = MCTs[{ BB, succ }];
        auto&& entries_A = MCT_A.entries(rank);
        /// We loop other all entries in the MCT of `(BB, succ)`
        for (auto itr = entries_A.begin(); itr != entries_A.end();) {
            auto& entry = *itr;
            /// The condition is always `true` for normal nodes and for landing
            /// pads it is all operands must not be defined in loop nodes.
            if (!condition(entry.copy())) {
                continue;
            }
            bool allOthersHaveSameEntry =
                ranges::all_of(BB->successors(), [&](BasicBlock* other) {
                    if (other == succ) {
                        return true;
                    }
                    auto& MCT_B = MCTs[{ BB, other }];
                    return MCT_B.hasComputationEqualTo(entry.copy());
                });
            if (!allOthersHaveSameEntry) {
                ++itr;
                continue;
            }
            auto* copy = insertIntoLTCAndBB(insertPoint, entry);
            for (auto* otherSucc: BB->successors()) {
                if (otherSucc == succ) {
                    continue;
                }
                auto& MCT_B = MCTs[{ BB, otherSucc }];
                auto* otherEntry = MCT_B.computationEqualTo(copy);
                for (auto* original: otherEntry->originals()) {
                    redundant.insert(original);
                    replaceValue(original, copy);
                    modified = true;
                }
                MCT_B.eraseComputationEqualTo(copy);
            }
            ++itr;
            MCT_A.erase(copy, &entry);
        }
    }
}

void GVNContext::moveOut(size_t rank,
                         BasicBlock* BB,
                         LocalComputationTable& LCT) {
    /// Identify candidates for movement to our predecessors
    auto movable = LCT.instructions(rank) | ranges::views::filter(isMoveable) |
                   ranges::to<utl::small_vector<Instruction*>>;
    switch (BB->numPredecessors()) {
    case 0:
        break;
    case 1: {
        auto* realPred = BB->singlePredecessor();
        auto allPreds = virtualPredecessors[BB];
        allPreds.push_back(realPred);
        for (auto* pred: allPreds) {
            auto& MCT = MCTs[{ pred, BB }];
            for (auto* inst: movable) {
                MCT.insert(rank, copyAndPhiRename(ctx, inst, realPred), inst);
            }
        }
        break;
    }
    default:
        /// Here we have multiple predecessors. Since the CFG at this point has
        /// no critical edges, we can assume that all our predecessors have only
        /// one successor, i.e. this block. This means it is guaranteed that all
        /// computations we put into the MCTs here will be moved into the
        /// predecessors. Therefore we can insert phi nodes here for the copies
        /// and replace the current value with the phi node.
        SC_ASSERT(virtualPredecessors[BB].empty(),
                  "Must be empty because loop exit nodes are branch nodes");
        for (auto* inst: movable) {
            utl::small_vector<PhiMapping> phiArgs;
            for (auto* pred: BB->predecessors()) {
                auto& MCT = MCTs[{ pred, BB }];
                auto copyOwner = copyAndPhiRename(ctx, inst, pred);
                auto* copy = copyOwner.get();
                MCT.insert(rank, std::move(copyOwner), inst);
                phiArgs.push_back({ pred, copy });
            }
            auto* phi = new Phi(phiArgs, std::string(inst->name()));
            BB->insertPhi(phi);
            replaceValue(inst, phi);
        }
        break;
    }
}
