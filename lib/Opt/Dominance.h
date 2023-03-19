#ifndef SCATHA_OPT_DOMINANCE_H_
#define SCATHA_OPT_DOMINANCE_H_

#include <iosfwd>
#include <span>

#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include "IR/Common.h"
#include "Opt/Graph.h"

namespace scatha::opt {

class DomTree;

using DominanceMap =
    utl::hashmap<ir::BasicBlock*, utl::hashset<ir::BasicBlock*>>;

SCATHA(API)
DominanceMap computeDominanceSets(ir::Function& function);

SCATHA(API)
DomTree buildDomTree(ir::Function& function, DominanceMap const& domSets);

class SCATHA(API) DomTree {
public:
    class Node: public opt::TreeNode<ir::BasicBlock*, Node> {
        using Base = opt::TreeNode<ir::BasicBlock*, Node>;

    public:
        using Base::Base;

        ir::BasicBlock* basicBlock() const { return payload(); }

    private:
        friend DomTree opt::buildDomTree(ir::Function&, DominanceMap const&);
    };

public:
    /// Construct an empty dominator tree.
    DomTree();

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
    friend DomTree opt::buildDomTree(ir::Function&, DominanceMap const&);

    Node& findMut(ir::BasicBlock const* bb) {
        return const_cast<Node&>(
            static_cast<DomTree const*>(this)->operator[](bb));
    }

private:
    using NodeSet = utl::hashset<Node, Node::PayloadHash, Node::PayloadEqual>;
    NodeSet _nodes;
    Node* _root;
};

SCATHA(API) void print(DomTree const& domTree);
SCATHA(API) void print(DomTree const& domTree, std::ostream& ostream);

using DominanceFrontierMap =
    utl::hashmap<ir::BasicBlock*, utl::small_vector<ir::BasicBlock*>>;

SCATHA(API)
DominanceFrontierMap computeDominanceFrontiers(ir::Function& function,
                                               DomTree const& domTree);

} // namespace scatha::opt

#endif // SCATHA_OPT_DOMINANCE_H_
