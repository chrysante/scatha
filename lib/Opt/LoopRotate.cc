/// # Before loop rotation
///
/// `H` is the loop header. If `H` has multiple predecessors that are not loop
/// nodes, a preheader is inserted so `H` has one edge coming from outside the
/// loop. `H` is expected to have one edge going into the loop and one edge
/// leaving the loop. `E` (entry) is the successor of `H` that is a loop node.
/// If that node has multiple predecessors a new node is inserted to take the
/// role of `E`. Nodes marked with `X` are loop nodes without any special role.
/// These could be exiting nodes. Exiting nodes can exit to `S` (skip block, the
/// other successor of `H`) or to other nodes. If `S` has multiple predecessors,
/// a new node will be inserted for `S`
///
// clang-format off
/// ```
///       ┌┴─┴┐           ┌───┐
///       │   ├──────────>│ S │
///   ┌──>│ H │<───┐      └───┘
///   │   └─┬─┘    │
///   │     ↓      │
///   │   ┌───┐    │
///   │   │ E │    │
///   │   └─┬─┘    │
///   │    ┌┴┐     │
///   │    │ │     │
///   │    │ │     │
/// ┌─┴─┐  │ │   ┌─┴─┐
/// │ X │<─┘ └──>│ X │
/// └─┬─┘        └───┘
///   │
///   ↓
/// ```
// clang-format on

/// # After loop rotation
///
/// `F` (footer) is a copy of `H`. `H` is renamed to `G` (guard). All
/// predecessors of `H` that are loop nodes now point to `F`. An edge from `F`
/// to `E` is added. This means now `E` has multiple predecessors and thus
/// (likely) has phi instructions.
///
// clang-format off
/// ```
///           ┌─┴─┐
///           │ G ├────────────┐
///           └─┬─┘            │
///             ↓              ↓
///           ┌───┐          ┌───┐
///           │ E │<───────┐ │ S │
///           └─┬─┘        │ └───┘
///            ┌┴┐         │   ↑
///            │ │         │   │
///            │ │         │   │
/// ┌───┐      │ │   ┌───┐ │   │
/// │ X │<─────┘ └──>│ X │ │   │
/// └┬┬─┘      ┌───┐ └─┬─┘ │   │
///  ││        │   │<──┘   │   │
///  │└───────>│ F │───────┘   │
///  ↓         │   │───────────┘
///            └───┘
///
/// ```
// clang-format on
///
/// # Implementation
///
/// ## Preprocessing
/// We add a preheader and a new nodes `E` and `S` if necessary.
///
/// ## Rotation
/// - We add single value phi instructions to `E` and to `S` for every
///   instruction in `H`. We replace all uses of the instructions in `H` that
///   are dominated by `E` and `S` with the corresponding phi instruction.
/// - We clone `H`, name the clone `F` and rename `H` to `G`. `F` now has the
///   same successors as `G`. This will remain so. We register `F` as a
///   predecessor of `E` and `S` and set all the phi instructions in these
///   blocks accordingly. In particular, there is one phi instruction for every
///   instruction in `G`. If previously there were other single valued phis in
///   `S` or `E` we replaced them by their argument. Therefore we can simply
///   add an entry to each phi instruction with the corresponding instruction in
///   `F`.
/// - All predecessors of `G` that are loop nodes are rewired to `F`. Since `F`
///   has a phi instruction for every phi in `G` we can keep these and use them
///   for the loop predecessors of `G`.
/// - After the rotation `E` is a loop header. If `E` is a while loop we perform
///   the preprocessing and transform again on `E`.
///

#include "Opt/Passes.h"

#include <queue>

#include <utl/hashtable.hpp>
#include <utl/strcat.hpp>
#include <utl/vector.hpp>

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

SC_REGISTER_CANONICALIZATION(opt::rotateLoops, "rotateloops");

/// \Returns `true` if \p node is a while loop
static bool isWhileLoop(LNFNode const* header, LoopNestingForest const& LNF) {
    if (!header->isProperLoop()) {
        return false;
    }
    if (header->children().empty()) {
        return false;
    }
    return ranges::any_of(header->basicBlock()->successors(),
                          [&](BasicBlock const* succ) {
        return !LNF[succ]->isLoopNodeOf(header);
    });
}

/// Erases all phi instructions in a basic block with one predecessor
static void eraseSingleValuePhiNodes(BasicBlock* BB) {
    SC_ASSERT(BB->numPredecessors() == 1, "");
    while (true) {
        auto* phi = dyncast<Phi*>(&BB->front());
        if (!phi) {
            break;
        }
        auto* arg = phi->argumentAt(0).value;
        replaceValue(phi, arg);
        BB->erase(phi);
    }
}

namespace {

struct PreprocessResult {
    BasicBlock* entry;
    BasicBlock* skip;
    utl::small_vector<BasicBlock*> headerLoopPreds;
    utl::small_vector<BasicBlock*> headerNonLoopPreds;
};

struct LRContext {
    Context& ctx;
    Function& function;
    LoopNestingForest& LNF;
    DominanceInfo const& domInfo;

    /// Used by `dominates()`
    utl::hashmap<BasicBlock*, BasicBlock*> ESMap;

    BasicBlock* mapES(BasicBlock* BB) const {
        auto itr = ESMap.find(BB);
        if (itr != ESMap.end()) {
            return itr->second;
        }
        return BB;
    }

    /// \Returns `true` if \p dom dominates \p sub
    /// Should only be used with `entry` and `skip` blocks
    bool dominates(BasicBlock* dom, BasicBlock* sub) const {
        dom = mapES(dom);
        sub = mapES(sub);
        auto& domSet = domInfo.domSet(sub);
        return domSet.contains(dom);
    }

    utl::small_vector<Phi*> addedPhis;

    LRContext(Context& ctx, Function& function):
        ctx(ctx),
        function(function),
        LNF(function.getOrComputeLNF()),
        domInfo(function.getOrComputeDomInfo()) {}

    void rotate(BasicBlock* header);

    PreprocessResult preprocess(BasicBlock* header);

    BasicBlock* addPreheader(BasicBlock* header,
                             std::span<BasicBlock* const> nonLoopPreds);

    void addSingleValuePhis(BasicBlock* header, BasicBlock* succ);

    void augmentSingleValuePhis(BasicBlock* footer, BasicBlock* succ);
};

struct TopSorter {
    Function const& function;
    utl::hashmap<BasicBlock const*, size_t> order;

    explicit TopSorter(Function const& function): function(function) {
        std::queue<BasicBlock const*> queue;
        queue.push(&function.entry());
        utl::hashset<BasicBlock const*> visited = { &function.entry() };
        size_t rank = 0;
        while (!queue.empty()) {
            auto* BB = queue.front();
            queue.pop();
            order[BB] = rank++;
            for (auto* succ: BB->successors()) {
                if (visited.insert(succ).second) {
                    queue.push(succ);
                }
            }
        }
    }

    size_t rank(BasicBlock const* BB) const {
        auto itr = order.find(BB);
        if (itr != order.end()) {
            return order.find(BB)->second;
        }
        return 0;
    }

    auto operator()(std::span<LNFNode const* const> nodes) const {
        auto result = nodes | ranges::to<utl::small_vector<LNFNode const*>>;
        std::sort(result.begin(), result.end(), [&](auto* A, auto* B) {
            return rank(A->basicBlock()) < rank(B->basicBlock());
        });
        return result;
    }
};

} // namespace

bool opt::rotateLoops(Context& ctx, Function& function) {
    /// We collect all the while loops of `function` in breadth first search
    /// order of the the loop nesting forest
    utl::small_vector<utl::small_vector<BasicBlock*>, 2> whileHeadersBFS;
    auto& LNF = function.getOrComputeLNF();
    auto topsort = TopSorter(function);
    auto DFS = [&](auto& DFS, LNFNode const* header, size_t index) -> void {
        if (isWhileLoop(header, LNF)) {
            if (index == whileHeadersBFS.size()) {
                whileHeadersBFS.emplace_back();
            }
            whileHeadersBFS[index].push_back(header->basicBlock());
            ++index;
        }
        for (auto* node: topsort(header->children())) {
            DFS(DFS, node, index);
        }
    };
    for (auto* root: topsort(LNF.roots())) {
        DFS(DFS, root, 0);
    }

    /// We traverse all while loops in rank order (BFS order)
    for (auto& rankList: whileHeadersBFS) {
        for (auto* header: rankList) {
            LRContext(ctx, function).rotate(header);
        }
        /// After traversing a rank we invalidate because we may have edited
        /// the CFG in loops the are dominated by the next rank
        function.invalidateCFGInfo();
    }

    assertInvariants(ctx, function);
    return !whileHeadersBFS.empty();
}

PreprocessResult LRContext::preprocess(BasicBlock* header) {
    LNFNode const* headerNode = LNF[header];

    /// Partition the predecessors of `header` into loop predecessors and
    /// non-loop predecessors.
    auto [loopPreds, nonLoopPreds] = [&] {
        std::array<utl::small_vector<BasicBlock*>, 2> result;
        auto& [loopPreds, nonLoopPreds] = result;
        for (auto* pred: header->predecessors()) {
            if (LNF[pred]->isLoopNodeOf(headerNode)) {
                loopPreds.push_back(pred);
            }
            else {
                nonLoopPreds.push_back(pred);
            }
        }
        return result;
    }();

    if (nonLoopPreds.size() > 1) {
        auto* preheader = addPreheader(header, nonLoopPreds);
        nonLoopPreds = { preheader };
    }

    /// We now determine which successor is `E` and which is `S`
    SC_ASSERT(header->numSuccessors() == 2,
              "If we have one successors this is either not a loop header or "
              "not a while loop and if we have more than 2 successors this is "
              "a weird switch based loop that we don't support.");
    auto [entry, skip] = [&] {
        auto A = header->successors()[0];
        auto B = header->successors()[1];
        if (LNF[A]->isLoopNodeOf(headerNode)) {
            return std::pair{ A, B };
        }
        return std::pair{ B, A };
    }();

    /// We add new nodes for `entry` and `skip` if necessary and make sure they
    /// have no phi nodes
    if (entry->numPredecessors() > 1) {
        auto* newEntry = splitEdge("loop.entry", ctx, header, entry);
        ESMap[newEntry] = entry;
        entry = newEntry;
    }
    else {
        eraseSingleValuePhiNodes(entry);
    }
    if (skip->numPredecessors() > 1) {
        auto* newSkip = splitEdge("loop.end", ctx, header, skip);
        ESMap[newSkip] = skip;
        skip = newSkip;
    }
    else {
        eraseSingleValuePhiNodes(skip);
    }

    return { entry, skip, loopPreds, nonLoopPreds };
}

BasicBlock* LRContext::addPreheader(BasicBlock* header,
                                    std::span<BasicBlock* const> nonLoopPreds) {
    return addJoiningPredecessor(ctx, header, nonLoopPreds, "preheader");
}

void LRContext::rotate(BasicBlock* header) {
    auto [entry, skip, loopPreds, nonLoopPreds] = preprocess(header);

    /// # Step 1
    /// `addSingleValuePhis()` replaces all uses of the instructions in the
    /// header that are dominated by either `entry` or `skip` with single value
    /// phi nodes in the respective block. The set of nodes dominated by
    /// `entry`, the set of nodes dominated by skip and `header` partition the
    /// dom set of the header. So all uses of instructions in the header that
    /// are not in in the header will be replaced by the single value phi nodes.
    addSingleValuePhis(header, entry);
    addSingleValuePhis(header, skip);

    /// # Step 2
    /// We clone the header and rename the nodes
    auto* footer = ir::clone(ctx, header).release();
    footer->setName("loop.footer");
    header->setName("loop.guard");
    function.insert(skip, footer);
    auto* guard = header;

    /// We add arguments for `footer` to the phi instructions of `entry` and
    /// `skip` and register `footer` as a predecessor
    entry->addPredecessor(footer);
    augmentSingleValuePhis(footer, entry);
    skip->addPredecessor(footer);
    augmentSingleValuePhis(footer, skip);

    /// # Step 3
    /// We remove all the loop predecessors of `guard` and point them to
    /// `footer`. `footer` already has phi instructions for the predecessors
    /// because `header` also had them
    for (auto* pred: loopPreds) {
        pred->terminator()->updateTarget(guard, footer);
        guard->removePredecessor(pred);
    }
    /// We unregister the non-loop predecessors from `footer`
    for (auto* pred: nonLoopPreds) {
        footer->removePredecessor(pred);
    }

    /// We check if `footer` has any self referential phi nodes. For `header` it
    /// was okay to have self referential phi nodes for blocks it dominated. But
    /// `footer` does not dominate any of the loop nodes, so we replace self
    /// referential arguments with the correspoding value in `entry`.
    utl::hashmap<Instruction*, Instruction*> FtoE;
    for (auto&& [F, E]: ranges::views::zip(*footer, *entry)) {
        FtoE[&F] = &E;
    }
    for (auto& phi: footer->phiNodes()) {
        for (auto [index, arg]: phi.operands() | ranges::views::enumerate) {
            auto* instArg = dyncast<Instruction*>(arg);
            if (instArg && instArg->parent() == footer) {
                phi.setOperand(index, FtoE[instArg]);
            }
        }
    }

    /// We remove all the phi nodes that were added for no reason
    for (auto* phi: addedPhis) {
        if (!phi->isUsed()) {
            phi->parent()->erase(phi);
        }
    }
}

void LRContext::addSingleValuePhis(BasicBlock* header, BasicBlock* succ) {
    for (auto& inst: *header) {
        if (isa<TerminatorInst>(inst)) {
            break;
        }
        utl::small_vector<Instruction*> dominatedUsers;
        for (auto* user: inst.users()) {
            if (!dominates(succ, user->parent())) {
                continue;
            }
            if (user->parent() == succ && isa<Phi>(user)) {
                continue;
            }
            dominatedUsers.push_back(user);
        }
        auto* phi = new Phi({ { header, &inst } }, std::string(inst.name()));
        succ->insertPhi(phi);
        for (auto* user: dominatedUsers) {
            user->updateOperand(&inst, phi);
        }
        addedPhis.push_back(phi);
    }
}

void LRContext::augmentSingleValuePhis(BasicBlock* footer, BasicBlock* succ) {
    /// This way of augmenting the phi nodes works because we have exactly one
    /// phi node in `succ` for every non-terminator instruction in `footer`
    for (auto&& [phi, inst]: ranges::views::zip(succ->phiNodes(), *footer)) {
        phi.addArgument(footer, &inst);
    }
}
