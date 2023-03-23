#ifndef SCATHA_IR_DOMINANCE_H_
#define SCATHA_IR_DOMINANCE_H_

#include <iosfwd>
#include <span>

#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include "Common/Graph.h"
#include "IR/Common.h"

namespace scatha::ir {

class DomTree;

/// Dominator tree of a function
class SCATHA_TESTAPI DomTree {
public:
    class Node: public TreeNode<ir::BasicBlock*, Node> {
        using Base = TreeNode<ir::BasicBlock*, Node>;

    public:
        using Base::Base;

        ir::BasicBlock* basicBlock() const { return payload(); }

    private:
        friend class DominanceInfo;
    };

public:
    /// \Returns Flat array of nodes in the dominator tree.
    auto nodes() const {
        return _nodes | ranges::views::transform(
                            [](auto& n) -> Node const& { return n; });
    }

    Node const& operator[](ir::BasicBlock const* bb) const {
        auto itr = _nodes.find(bb);
        SC_ASSERT(itr != _nodes.end(), "Not found");
        return *itr;
    }

    /// \Returns root of the dominator tree.
    Node const& root() const { return *_root; }

    ir::BasicBlock* idom(ir::BasicBlock* block) const {
        return const_cast<ir::BasicBlock*>(
            idom(static_cast<ir::BasicBlock const*>(block)));
    }

    ir::BasicBlock const* idom(ir::BasicBlock const* block) const {
        return (*this)[block].parent().basicBlock();
    }

private:
    friend class DominanceInfo;

    Node& findMut(ir::BasicBlock const* bb) {
        return const_cast<Node&>(
            static_cast<DomTree const*>(this)->operator[](bb));
    }

private:
    using NodeSet = utl::hashset<Node, Node::PayloadHash, Node::PayloadEqual>;
    NodeSet _nodes;
    Node* _root;
};

SCATHA_TESTAPI void print(DomTree const& domTree);

SCATHA_TESTAPI void print(DomTree const& domTree, std::ostream& ostream);

/// Groups dominance information of a function.
/// Specifically, once computed, it contains:
/// - Dominance sets for each basic block
/// - A dominator tree
/// - Dominance frontiers for each basic block
class SCATHA_TESTAPI DominanceInfo {
public:
    using DomMap = utl::hashmap<ir::BasicBlock*, utl::hashset<ir::BasicBlock*>>;

    using DomFrontMap =
        utl::hashmap<ir::BasicBlock*, utl::small_vector<ir::BasicBlock*>>;

    /// Compute the dominator sets of the basic blocks in \p function
    /// I.e. for each basic block `B` the set of basic blocks the dominate `B`.
    static DomMap computeDomSets(ir::Function& function);

    /// Compute the dominator tree of \p function
    static DomTree computeDomTree(ir::Function& function, DomMap const& domMap);

    /// Compute the dominance frontiers of the basic blocks in \p function
    static DomFrontMap computeDomFronts(ir::Function& function,
                                        DomTree const& domTree);

    /// Compute dominance information of \p function
    /// Computes dominance sets, dominator tree and dominance frontiers.
    static DominanceInfo compute(ir::Function& function);

    /// \returns a reference to the set of basic blocks dominated by \p
    /// basicBlock
    utl::hashset<ir::BasicBlock*> const& domSet(
        ir::BasicBlock const* basicBlock) const;

    /// \returns the dominator tree
    DomTree const& domTree() const { return _domTree; }

    /// \returns a view of the dominance frontier of \p basicBlock
    std::span<ir::BasicBlock* const> domFront(
        ir::BasicBlock const* basicBlock) const;

private:
    friend struct DFContext;

private:
    DomMap _domMap;
    DomTree _domTree;
    DomFrontMap _domFront;
};

} // namespace scatha::ir

#endif // SCATHA_IR_DOMINANCE_H_