#ifndef SCATHA_IR_LOOP_H_
#define SCATHA_IR_LOOP_H_

#include <iosfwd>
#include <span>

#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include "Common/Graph.h"
#include "IR/Fwd.h"

namespace scatha::ir {

class DomTree;
class LNFNode;

/// Loop metadata structure
class LoopInfo {
public:
    /// Construct an empty loop info object
    LoopInfo() = default;

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

    /// \Returns a view over all basic blocks that the loop may exit from
    auto const& exitingBlocks() const { return _exitingBlocks; }

    /// \Returns `true` if \p BB is an exiting block of this loop
    bool isExiting(BasicBlock const* BB) const {
        return _exitingBlocks.contains(BB);
    }

    /// \Returns a view over all basic blocks that the loop may exit to
    auto const& exitBlocks() const { return _exitBlocks; }

    /// \Returns `true` if \p BB is an exit block of this loop
    bool isExit(BasicBlock const* BB) const { return _exitBlocks.contains(BB); }

    /// \Returns a list of all loop closing phi instructions in the exit block
    /// \p exit
    std::span<Phi* const> loopClosingPhiNodes(BasicBlock const* exit) const;

    /// \Returns a list of the induction variables of this loop
    std::span<Instruction* const> inductionVariables() const {
        return _inductionVars;
    }

private:
    friend void makeLCSSA(LoopInfo& loopInfo);

    BasicBlock* _header = nullptr;
    utl::hashset<BasicBlock*> _innerBlocks;
    utl::hashset<BasicBlock*> _exitingBlocks;
    utl::hashset<BasicBlock*> _exitBlocks;
    utl::hashmap<BasicBlock const*, utl::small_vector<Phi*>>
        _loopClosingPhiNodes;
    utl::small_vector<Instruction*, 2> _inductionVars;
};

/// \Returns `true` if the loop \p loop is in LCSSA form
bool isLCSSA(LoopInfo const& loop);

/// Turns the function \p function into LCSSA form
/// \Returns `true` if \p function has been modified
void makeLCSSA(Function& function);

/// Turns the loop described by \p loopInfo into LCSSA form
/// \Returns `true` if the loop has been modified
void makeLCSSA(LoopInfo& loopInfo);

/// Node in the loop nesting forest. Every node directly corresponds to one
/// basic block.
class LNFNode: public GraphNode<ir::BasicBlock*, LNFNode, GraphKind::Tree> {
public:
    using GraphNode::GraphNode;

    /// \Returns the corresponding basic block
    BasicBlock* basicBlock() const { return payload(); }

    /// \Returns `true` if this node is an actual loop, i.e. if it has children
    /// or if its corresponding basic block has outedges to itself
    bool isProperLoop() const;

    /// \Returns `true` if this node is part of the loop with header \p header
    bool isLoopNodeOf(LNFNode const* header) const;

    ///
    LoopInfo& loopInfo() {
        if (!_loopInfo) {
            _loopInfo = std::make_unique<LoopInfo>(LoopInfo::Compute(*this));
        }
        return *_loopInfo;
    }

    std::unique_ptr<LoopInfo> _loopInfo;
};

/// The loop nesting forest of a function `F` is a forest representing the loops
/// of `F`. Every node is the header of a loop, where single basic blocks are
/// considered to be trivial loops.
class SCTEST_API LoopNestingForest {
public:
    using Node = LNFNode;

    /// Compute the loop nesting forest of \p function
    static LoopNestingForest compute(ir::Function& function,
                                     DomTree const& domtree);

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
    auto roots() const { return _virtualRoot->children(); }

    /// \Returns `true` if the forest is empty.
    bool empty() const { return _nodes.empty(); }

    /// Add a new node without children for basic block \p BB as child of node
    /// \p parent
    void addNode(Node const* parent, BasicBlock* BB);

    /// \overload
    void addNode(BasicBlock const* parent, BasicBlock* BB);

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

private:
    Node* findMut(ir::BasicBlock const* bb) {
        return const_cast<Node*>(
            static_cast<LoopNestingForest const*>(this)->operator[](bb));
    }

    using NodeSet =
        utl::node_hashset<Node, Node::PayloadHash<>, Node::PayloadEqual<>>;
    NodeSet _nodes;
    std::unique_ptr<Node> _virtualRoot;
};

/// Print the loop nesting forest \p LNF to `std::cout`.
SCTEST_API void print(LoopNestingForest const& LNF);

/// Print the loop nesting forest \p LNF to \p ostream
SCTEST_API void print(LoopNestingForest const& LNF, std::ostream& ostream);

} // namespace scatha::ir

#endif // SCATHA_IR_LOOP_H_
