#include "Opt/Passes.h"

#include <array>
#include <bit>
#include <queue>

#include <iostream>

#include <range/v3/algorithm.hpp>
#include <utl/graph.hpp>
#include <utl/hash.hpp>
#include <utl/hashtable.hpp>
#include <utl/ilist.hpp>

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

struct Computation {
    Computation(Instruction const* inst): inst(inst) {}

    bool operator==(Computation const& RHS) const {
        if (inst->nodeType() != RHS.inst->nodeType()) {
            return false;
        }
        // clang-format off
        bool const compEqual = utl::visit(*inst, utl::overload{
            [&](ArithmeticInst const& inst) {
                return inst.operation() ==
                       cast<ArithmeticInst const*>(RHS.inst)->operation();
            },
            [&](UnaryArithmeticInst const& inst) {
                return inst.operation() ==
                       cast<UnaryArithmeticInst const*>(RHS.inst)->operation();
            },
            [&](CompareInst const& inst) {
                return inst.operation() ==
                       cast<CompareInst const*>(RHS.inst)->operation();
            },
            [&](ExtractValue const& inst) {
                return ranges::equal(
                           inst.memberIndices(),
                           cast<ExtractValue const*>(RHS.inst)->memberIndices());
            },
            [&](InsertValue const& inst) {
                return ranges::equal(
                           inst.memberIndices(),
                           cast<InsertValue const*>(RHS.inst)->memberIndices());
            },
            [&](Instruction const& inst) -> bool {
                SC_UNREACHABLE();
            }
        }); // clang-format on
        if (!compEqual) {
            return false;
        }
        return ranges::equal(inst->operands(), RHS.inst->operands());
    }

    size_t hashValue() const {
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
            [&](Instruction const& inst) {
                SC_UNREACHABLE();
            }
        }); // clang-format on
        for (auto* operand: inst->operands()) {
            utl::hash_combine_seed(seed, operand);
        }
        return seed;
    }

private:
    Instruction const* inst;
};

} // namespace

template <>
struct std::hash<Computation> {
    size_t operator()(Computation expr) const { return expr.hashValue(); }
};

namespace {

/// A local computation table exists for every block and holds the computations
/// of that basic block in a way that we can conveniently
/// - loop over all computations
/// - loop over all computations of a given rank
/// - identify all identical computations for a given computation
class LocalComputationTable {
public:
    static LocalComputationTable build(
        BasicBlock* BB, utl::hashmap<Instruction*, size_t> const& ranks);

    /// Insert a computation into the table
    void insert(size_t rank, Instruction* inst) {
        rankMap[rank].push_back(inst);
    }

    /// View over all computations
    auto computations() const {
        return rankMap | ranges::views::transform([](auto& p) -> auto& {
                   return p.second;
               }) |
               ranges::views::join;
    }

    /// View over all computations of a given rank
    std::span<Instruction* const> computations(size_t rank) const {
        auto itr = rankMap.find(rank);
        if (itr != rankMap.end()) {
            return itr->second;
        }
        return {};
    }

    /// The maximum rank of computations in this LCT
    size_t maxRank() const { return _maxRank; }

private:
    utl::hashmap<size_t, utl::small_vector<Instruction*>> rankMap;
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
    bool hasComputationEqualTo(Instruction const* inst) const {
        return compMap.contains(Computation(inst));
    }

    /// Erase all computations equal to \p inst
    void eraseComputationEqualTo(Instruction const* inst) {
        auto itr = compMap.find(Computation(inst));
        if (itr == compMap.end()) {
            return;
        }
        auto* entry = itr->second;
        compMap.erase(itr);
        entry->list->erase(entry);
    }

    auto computationEqualTo(Instruction const* inst) const {
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
    utl::hashmap<BasicBlock*, RankMap<Instruction>> localRanks;
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

    /// The main algorithm
    void processHeader(size_t rank, BasicBlock* BB);
    /// \Returns `true` if the computation \p inst can be moved out of a loop
    /// header
    bool isHeaderMovable(Instruction* inst);

    void processLandingPad(size_t rank, BasicBlock* BB);
    void processOther(size_t rank, BasicBlock* BB);
    void moveIn(size_t rank, BasicBlock* BB);

    Instruction* insertPointForRank(BasicBlock* BB, size_t rank) {
        auto nextRank = LCTs[BB].computations(rank + 1);
        if (!nextRank.empty()) {
            return nextRank.back();
        }
        return BB->terminator();
    }

    void buildLCTs();
    void elimLocal();
    size_t computeRank(Instruction* inst);
    size_t getAvailRank(Value* value);
};

} // namespace

namespace scatha::opt {

void print(GVNContext const* ctx) { std::cout << "Info\n"; }

} // namespace scatha::opt

bool opt::globalValueNumbering(Context& ctx, Function& function) {
    bool result = false;
    result |= rotateLoops(ctx, function);
    GVNContext gvnContext(ctx, function);
    result |= gvnContext.run();
    assertInvariants(ctx, function);
    return result;
}

bool GVNContext::run() {
    splitCriticalEdges();
    gatherLoops();
    computeTopsortOrder();
    assignRanks();
#if 1
    std::cout << "Virtual edges: \n";
    for (auto& [exit, preds]: virtualPredecessors) {
        for (auto* pred: preds) {
            std::cout << "    " << pred->name() << " --> " << exit->name()
                      << std::endl;
        }
    }
    std::cout << "Ranks: \n";
    for (auto [value, rank]: globalRanks) {
        std::cout << "    " << value->name() << " : " << rank << std::endl;
    }
#endif
    for (size_t rank = 0; rank <= maxRank; ++rank) {
        for (auto* BB: topsortOrder) {
            if (loopHeaders.contains(BB)) {
                processHeader(rank, BB);
            }
            else if (landingPads.contains(BB)) {
                processLandingPad(rank, BB);
            }
            else {
                processOther(rank, BB);
            }
        }
    }

    //    joinSplitEdges();
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
        auto& loop = loops[header] = Loop{ header, landingPad };
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
        utl::hashset<std::pair<BasicBlock*, BasicBlock*>> backEdges;

        void search(BasicBlock* BB) {
            visited.insert(BB);
            for (auto* succ: BB->successors()) {
                if (visited.contains(succ)) {
                    backEdges.insert({ BB, succ });
                    continue;
                }
                search(succ);
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
    for (auto* BB: topsortOrder | ranges::views::reverse) {
        for (auto& inst: *BB) {
            if (isa<TerminatorInst>(inst)) {
                continue;
            }
            size_t rank = computeRank(&inst);
            globalRanks[&inst] = rank;
            localRanks[BB][&inst] = rank;
            maxRank = std::max(maxRank, rank);
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

static auto isMoveable = [](Instruction* inst) {
    /// Here we should probably also check for certain side effects
    if (isa<Phi>(inst) || isa<TerminatorInst>(inst)) {
        return false;
    }
    for (auto* operand: inst->operands()) {
        auto* instOp = dyncast<Instruction*>(operand);
        if (!instOp) {
            continue;
        }
        if (isa<Phi>(instOp)) {
            continue;
        }
        if (instOp->parent() == inst->parent()) {
            return false;
        }
    }
    return true;
};

static UniquePtr<Instruction> copyForPred(Context& ctx,
                                          Instruction* inst,
                                          BasicBlock* pred) {
    auto copy = ir::clone(ctx, inst);
    for (auto [index, operand]: inst->operands() | ranges::views::enumerate) {
        if (auto* phi = dyncast<Phi*>(operand)) {
            copy->updateOperand(phi, phi->operandOf(pred));
        }
    }
    return copy;
}

void GVNContext::processHeader(size_t rank, BasicBlock* BB) {
    moveIn(rank, BB);
}

bool GVNContext::isHeaderMovable(Instruction* inst) {}

void GVNContext::processLandingPad(size_t rank, BasicBlock* BB) {
    SC_UNIMPLEMENTED();
}

void GVNContext::processOther(size_t rank, BasicBlock* BB) {
    moveIn(rank, BB);

    /// Identify candidates for movement to our predecessors
    auto& LCT = LCTs[BB];
    auto movable = LCT.computations(rank) | ranges::views::filter(isMoveable) |
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
                MCT.insert(rank, copyForPred(ctx, inst, realPred), inst);
            }
        }
        break;
    }
    default: {
        /// Here we have multiple predecessors. Since the CFG at this point has
        /// no critical edges, we can assume that all our predecessors have only
        /// one successor, i.e. this block. This means it is guaranteed that all
        /// computations we put into the MCTs here will be moved into the
        /// predecessors. Therefore we can insert phi nodes here for the copies
        /// and replace the current value with the phi node.
        SC_ASSERT(virtualPredecessors[BB].empty(),
                  "Must be empty according to the paper");
        for (auto* inst: movable) {
            utl::small_vector<PhiMapping> phiArgs;
            for (auto* pred: BB->predecessors()) {
                auto& MCT = MCTs[{ pred, BB }];
                auto copyOwner = copyForPred(ctx, inst, pred);
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
}

void GVNContext::moveIn(size_t rank, BasicBlock* BB) {
    auto& LCT = LCTs[BB];
    if (rank == 0) {
        LCT = LocalComputationTable::build(BB, localRanks[BB]);
    }
    /// Move computations from successors into this block
    if (BB->numSuccessors() == 1) {
        auto& MCT = MCTs[{ BB, BB->singleSuccessor() }];
        auto insertPoint = insertPointForRank(BB, rank);
        for (auto& entry: MCT.entries(rank)) {
            auto* copy = entry.takeCopy();
            BB->insert(insertPoint, copy);
            LCT.insert(rank, copy);
        }
        MCT.clear();
    }
    else {
        auto insertPoint = insertPointForRank(BB, rank);
        for (auto* succ: BB->successors()) {
            auto& MCT_A = MCTs[{ BB, succ }];
            auto&& entries_A = MCT_A.entries(rank);
            for (auto itr = entries_A.begin(); itr != entries_A.end();) {
                auto& entry = *itr;
                bool allOthersHave = true;
                for (auto* otherSucc: BB->successors()) {
                    if (otherSucc != succ) {
                        auto& MCT_B = MCTs[{ BB, otherSucc }];
                        allOthersHave &=
                            MCT_B.hasComputationEqualTo(entry.copy());
                    }
                }
                if (!allOthersHave) {
                    ++itr;
                    continue;
                }
                auto* copy = entry.takeCopy();
                BB->insert(insertPoint, copy);
                for (auto* original: entry.originals()) {
                    replaceValue(original, copy);
                }
                for (auto* otherSucc: BB->successors()) {
                    if (otherSucc != succ) {
                        auto& MCT_B = MCTs[{ BB, otherSucc }];
                        auto* otherEntry = MCT_B.computationEqualTo(copy);
                        for (auto* original: otherEntry->originals()) {
                            replaceValue(original, copy);
                        }
                        MCT_B.eraseComputationEqualTo(copy);
                    }
                }
                ++itr;
                MCT_A.erase(copy, &entry);
            }
        }
    }
}

void GVNContext::elimLocal() {
#if 0
    for (auto& BB: function) {
        auto& LCT = LCTs.find(&BB)->second;
        for (size_t rank = 0; rank < LCT.maxRank(); ++rank) {
            auto& comps = LCT.computations(rank);
            for (auto i = comps.begin(); i != comps.end(); ++i) {
                for (auto j = i + 1; j != comps.end();) {
                    bool canErase =
                        Computation(*i) == Computation(*j) &&
                        ranges::equal((*i)->operands(), (*j)->operands()) &&
                        !hasSideEffects(*i);
                    if (canErase) {
                        replaceValue(*j, *i);
                        BB.erase(*j);
                        j = comps.erase(j);
                        continue;
                    }
                    ++j;
                }
            }
        }
    }
#endif
}

LocalComputationTable LocalComputationTable::build(
    BasicBlock* BB, utl::hashmap<Instruction*, size_t> const& ranks) {
    LocalComputationTable result;
    for (auto [inst, rank]: ranks) {
        result.rankMap[rank].push_back(inst);
        result._maxRank = std::max(result._maxRank, rank);
    }
    return result;
}
