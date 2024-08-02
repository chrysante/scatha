#ifndef SCATHA_IR_LOOP_H_
#define SCATHA_IR_LOOP_H_

#include <iosfwd>
#include <memory>
#include <span>

#include <utl/function_view.hpp>
#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include "Common/Base.h"
#include "Common/Graph.h"
#include "Common/Ranges.h"
#include "IR/Fwd.h"

namespace scatha::ir {

class DomTree;
class LNFNode;

/// Loop metadata structure
class LoopInfo {
public:
    /// Construct an empty loop info object
    LoopInfo() = default;

    /// Construct a loop info object from all its members
    LoopInfo(
        BasicBlock* header, utl::hashset<BasicBlock*> innerBlocks,
        utl::hashset<BasicBlock*> enteringBlocks,
        utl::hashset<BasicBlock*> latches,
        utl::hashset<BasicBlock*> exitingBlocks,
        utl::hashset<BasicBlock*> exitBlocks,
        utl::hashmap<std::pair<BasicBlock const*, Instruction const*>, Phi*>
            loopClosingPhiNodes,
        std::span<Instruction* const> inductionVars):
        _header(header),
        _innerBlocks(std::move(innerBlocks)),
        _enteringBlocks(std::move(enteringBlocks)),
        _latches(std::move(latches)),
        _exitingBlocks(std::move(exitingBlocks)),
        _exitBlocks(std::move(exitBlocks)),
        _loopClosingPhiNodes(std::move(loopClosingPhiNodes)),
        _inductionVars(inductionVars | ToSmallVector<>) {}

    /// Compute the loop info metadata from the loop nesting forst node \p
    /// header
    static LoopInfo Compute(LNFNode const& header);

    /// \Returns the header basic block
    BasicBlock* header() const { return _header; }

    /// \Returns the parent function of this loop
    Function* function() const;

    /// \Returns a view over all (inner) basic blocks in the loop
    auto const& innerBlocks() const { return _innerBlocks; }

    /// \Returns `true` if \p BB is an inner block of this loop
    bool isInner(BasicBlock const* BB) const {
        return _innerBlocks.contains(BB);
    }

    /// \Returns a view over all basic blocks that may enter the loop
    auto const& enteringBlocks() const { return _enteringBlocks; }

    /// \Returns `true` if \p BB is an entering block of this loop
    bool isEntering(BasicBlock const* BB) const {
        return _enteringBlocks.contains(BB);
    }

    /// \Returns a view over all inner basic blocks that have an edge to the
    /// header
    auto const& latches() const { return _latches; }

    /// \Returns `true` if \p BB is an inner block that has an edge to the
    /// header
    bool isLatch(BasicBlock const* BB) const { return _latches.contains(BB); }

    /// \Returns a view over all basic blocks that the loop may exit from
    auto const& exitingBlocks() const { return _exitingBlocks; }

    /// \Returns `true` if \p BB is an exiting block of this loop, i.e. in inner
    /// block that has an edge to an outer block
    bool isExiting(BasicBlock const* BB) const {
        return _exitingBlocks.contains(BB);
    }

    /// \Returns a view over all basic blocks that the loop may exit to
    auto const& exitBlocks() const { return _exitBlocks; }

    /// \Returns `true` if \p BB is an exit block of this loop, i.e. an outer
    /// block that has an incoming edge from a loop block
    bool isExit(BasicBlock const* BB) const { return _exitBlocks.contains(BB); }

    /// \Returns the phi node that closes the live out value \p loopInst for the
    /// exit block \p exit or null if \p loopInst is not live out or if \p exit
    /// is not an exit block
    Phi* loopClosingPhiNode(BasicBlock const* exit,
                            Instruction const* loopInst) const;

    ///
    auto const& loopClosingPhiMap() const { return _loopClosingPhiNodes; }

    /// \Returns a list of the induction variables of this loop
    std::span<Instruction* const> inductionVariables() const {
        return _inductionVars;
    }

private:
    friend bool makeLCSSA(LoopInfo& loopInfo);

    BasicBlock* _header = nullptr;
    utl::hashset<BasicBlock*> _innerBlocks;
    utl::hashset<BasicBlock*> _enteringBlocks;
    utl::hashset<BasicBlock*> _latches;
    utl::hashset<BasicBlock*> _exitingBlocks;
    utl::hashset<BasicBlock*> _exitBlocks;
    utl::hashmap<std::pair<BasicBlock const*, Instruction const*>, Phi*>
        _loopClosingPhiNodes;
    utl::small_vector<Instruction*, 2> _inductionVars;
};

/// Print loop info \p loop to \p ostream
void print(LoopInfo const& loop, std::ostream& ostream);

/// Print loop info \p loop to `std::cout`
void print(LoopInfo const& loop);

/// \Returns `true` if the loop \p loop is in LCSSA form
bool isLCSSA(LoopInfo const& loop);

/// Turns the function \p function into LCSSA form
/// \Returns `true` if \p function has been modified
bool makeLCSSA(Function& function);

/// Turns the loop described by \p loopInfo into LCSSA form
/// \Returns `true` if the loop has been modified
bool makeLCSSA(LoopInfo& loopInfo);

/// Canonicalizes \p loop to have a preheader, a single backedge and dedicated
/// exit blocks
bool simplifyLoop(Context& ctx, LNFNode& loop);

/// Node in the loop nesting forest. Every node directly corresponds to one
/// basic block.
/// TODO: Rename to `Loop`
class LNFNode: public GraphNode<ir::BasicBlock*, LNFNode, GraphKind::Tree> {
public:
    explicit LNFNode(ir::BasicBlock* BB): GraphNode(BB) { SC_EXPECT(BB); }

    /// \Returns the corresponding basic block
    BasicBlock* basicBlock() const { return payload(); }

    /// \Returns `true` if this node is an actual loop, i.e. if it has children
    /// or if its corresponding basic block has outedges to itself
    bool isProperLoop() const;

    /// \Returns `true` if this node is part of the loop with header \p header
    bool isLoopNodeOf(LNFNode const* header) const;

    /// Lazily computes loop info for this node
    LoopInfo& loopInfo() {
        return const_cast<LoopInfo&>(std::as_const(*this).loopInfo());
    }

    /// \overload
    LoopInfo const& loopInfo() const {
        if (!_loopInfo) {
            _loopInfo = std::make_unique<LoopInfo>(LoopInfo::Compute(*this));
        }
        return *_loopInfo;
    }

    ///
    void invalidateLoopInfo() { _loopInfo = nullptr; }

    /// \Returns the owning LNF
    LoopNestingForest& getLNF() {
        return const_cast<LoopNestingForest&>(std::as_const(*this).getLNF());
    }

    /// \overload
    LoopNestingForest const& getLNF() const;

private:
    friend class LoopNestingForest;

    LNFNode(std::nullptr_t): GraphNode() {}

    mutable std::unique_ptr<LoopInfo> _loopInfo;
};

/// The loop nesting forest of a function `F` is a forest representing the loops
/// of `F`. Every node is the header of a loop, where single basic blocks are
/// considered to be trivial loops.
class SCTEST_API LoopNestingForest: private LNFNode {
public:
    using Node = LNFNode;

    /// Compute the loop nesting forest of \p function
    static std::unique_ptr<LoopNestingForest> compute(ir::Function& function,
                                                      DomTree const& domtree);

    LoopNestingForest(LoopNestingForest const&) = delete;
    LoopNestingForest& operator=(LoopNestingForest const&) = delete;

    /// \Returns The node corresponding to basic block \p BB
    Node* operator[](ir::BasicBlock const* BB) {
        return const_cast<Node*>(
            static_cast<LoopNestingForest const&>(*this)[BB]);
    }

    ///  \overload
    Node const* operator[](ir::BasicBlock const* BB) const {
        auto itr = _nodes.find(BB);
        SC_ASSERT(itr != _nodes.end(), "Not found");
        return &*itr;
    }

    /// \Returns roots of the forest.
    std::span<LNFNode* const> roots() { return LNFNode::children(); }

    /// \overload
    std::span<LNFNode const* const> roots() const {
        return LNFNode::children();
    }

    /// \Returns `true` if the forest is empty.
    bool empty() const { return _nodes.empty(); }

    /// Add a new node without children for basic block \p BB as child of node
    /// \p parent
    void addNode(Node const* parent, BasicBlock* BB);

    /// \overload
    void addNode(BasicBlock const* parent, BasicBlock* BB);

    /// \Returns the number of nodes
    size_t numNodes() const { return _nodes.size(); }

    /// Traverse the forest in BFS order
    template <std::invocable<Node const*> F>
    void BFS(F&& f) const {
        LNFNode::BFS([&](Node const* node) {
            if (node != this) {
                f(node);
            }
        });
    }

    /// \overload
    template <std::invocable<Node*> F>
    void BFS(F&& f) {
        LNFNode::BFS([&](Node* node) {
            if (node != this) {
                f(node);
            }
        });
    }

    /// Traverse all trees in preorder
    template <std::invocable<Node const*> F>
    void preorderDFS(F&& f) const {
        for (auto* root: roots()) {
            root->preorderDFS(f);
        }
    }

    /// \overload
    template <std::invocable<Node*> F>
    void preorderDFS(F&& f) {
        for (auto* root: roots()) {
            root->preorderDFS(f);
        }
    }

    /// Traverse all trees in postorder
    template <std::invocable<Node const*> F>
    void postorderDFS(F&& f) const {
        for (auto* root: roots()) {
            root->postorderDFS(f);
        }
    }

    /// \overload
    template <std::invocable<Node*> F>
    void postorderDFS(F&& f) {
        for (auto* root: roots()) {
            root->postorderDFS(f);
        }
    }

    ///
    LNFNode* virtualRoot() { return this; }

    /// \overload
    LNFNode const* virtualRoot() const { return this; }

private:
    LoopNestingForest(): LNFNode(nullptr) {}

    Node* findMut(ir::BasicBlock const* bb) {
        return const_cast<Node*>(
            static_cast<LoopNestingForest const*>(this)->operator[](bb));
    }

    using NodeSet =
        utl::node_hashset<Node, Node::PayloadHash<>, Node::PayloadEqual<>>;

    friend class LNFNode;

    NodeSet _nodes;
};

/// Compares the loop nest forests \p A and \p B for equality. If a difference
/// is detected, \p diffCallback is invoked on the differing basic blocks
bool compareEqual(
    LoopNestingForest const& A, LoopNestingForest const& B,
    utl::function_view<void(LNFNode const&, LNFNode const&)> diffCallback);

/// Print the loop nesting forest \p LNF to `std::cout`.
SCTEST_API void print(LoopNestingForest const& LNF);

/// Print the loop nesting forest \p LNF to \p ostream
SCTEST_API void print(LoopNestingForest const& LNF, std::ostream& ostream);

} // namespace scatha::ir

#endif // SCATHA_IR_LOOP_H_
