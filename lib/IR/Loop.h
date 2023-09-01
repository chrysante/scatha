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

class LNFNode: public GraphNode<ir::BasicBlock*, LNFNode, GraphKind::Tree> {
    using Base = GraphNode<ir::BasicBlock*, LNFNode, GraphKind::Tree>;

public:
    using Base::Base;

    /// \Returns the corresponding basic block
    BasicBlock* basicBlock() const { return payload(); }

    /// \Returns `true` if this node is an actual loop, i.e. if it has children
    /// or if its corresponding basic block has outedges to itself
    bool isProperLoop() const;

    /// \Returns `true` if this node is part of the loop with header \p header
    bool isLoopNodeOf(LNFNode const* header) const;
};

/// The loop nesting forest of a function `F` is a forest representing the loops
/// of `F`. Every node is the header of a loop, where single basic blocks are
/// considered to be trivial loops.
class SCATHA_TESTAPI LoopNestingForest {
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
    void traversePreorder(F&& f) const {
        for (auto* root: roots()) {
            root->traversePreorder(f);
        }
    }

    /// \overload
    template <std::invocable<Node*> F>
    void traversePreorder(F&& f) {
        for (auto* root: roots()) {
            root->traversePreorder(f);
        }
    }

private:
    Node* findMut(ir::BasicBlock const* bb) {
        return const_cast<Node*>(
            static_cast<LoopNestingForest const*>(this)->operator[](bb));
    }

    using NodeSet =
        utl::node_hashset<Node, Node::PayloadHash, Node::PayloadEqual>;
    NodeSet _nodes;
    std::unique_ptr<Node> _virtualRoot;
};

/// Print the loop nesting forest \p LNF to `std::cout`.
SCATHA_TESTAPI void print(LoopNestingForest const& LNF);

/// Print the loop nesting forest \p LNF to \p ostream
SCATHA_TESTAPI void print(LoopNestingForest const& LNF, std::ostream& ostream);

} // namespace scatha::ir

#endif // SCATHA_IR_LOOP_H_
