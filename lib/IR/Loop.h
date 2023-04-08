#ifndef SCATHA_IR_LOOP_H_
#define SCATHA_IR_LOOP_H_

#include <iosfwd>
#include <span>

#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include "Common/Graph.h"
#include "IR/Common.h"

namespace scatha::ir {

class DomTree;

/// The loop nesting forest of a function `F` is a forest representing the loops
/// of `F`. Every node is the header of a loop, where single basic blocks are
/// considered to be trivial loops.
class SCATHA_TESTAPI LoopNestingForest {
public:
    class Node: public GraphNode<ir::BasicBlock*, Node, GraphKind::Tree> {
        using Base = GraphNode<ir::BasicBlock*, Node, GraphKind::Tree>;

    public:
        using Base::Base;

        BasicBlock* basicBlock() const { return payload(); }
    };

    /// Compute the loop nesting forest of \p function
    static LoopNestingForest compute(ir::Function& function,
                                     DomTree const& domtree);

    /// \Returns The node corresponding to basic block \p BB
    Node const* operator[](ir::BasicBlock const* BB) const {
        auto itr = _nodes.find(BB);
        SC_ASSERT(itr != _nodes.end(), "Not found");
        return &*itr;
    }

    /// \Returns roots of the forest.
    auto roots() const { return _virtualRoot.children(); }

    /// \Returns `true` iff the forest is empty.
    bool empty() const { return _nodes.empty(); }

private:
    Node* findMut(ir::BasicBlock const* bb) {
        return const_cast<Node*>(
            static_cast<LoopNestingForest const*>(this)->operator[](bb));
    }

    using NodeSet = utl::hashset<Node, Node::PayloadHash, Node::PayloadEqual>;
    NodeSet _nodes;
    Node _virtualRoot;
};

/// Print the loop nesting forest \p LNF to `std::cout`.
SCATHA_TESTAPI void print(LoopNestingForest const& LNF);

/// Print the loop nesting forest \p LNF to \p ostream
SCATHA_TESTAPI void print(LoopNestingForest const& LNF, std::ostream& ostream);

} // namespace scatha::ir

#endif // SCATHA_IR_LOOP_H_
