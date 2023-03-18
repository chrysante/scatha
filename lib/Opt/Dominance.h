#ifndef SCATHA_OPT_DOMINANCE_H_
#define SCATHA_OPT_DOMINANCE_H_

#include <iosfwd>
#include <span>

#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include "IR/Common.h"

namespace scatha::opt {

class DomTree;

SCATHA(API) DomTree buildDomTree(ir::Function& function);

class SCATHA(API) DomTree {
public:
    class Node {
    public:
        explicit Node(ir::BasicBlock* basicBlock):
            _bb(basicBlock) {}
        
        ir::BasicBlock* basicBlock() const { return _bb; }
        
        Node* parent() const { return _parent; }
        
        auto children() const {
            return _children | ranges::views::transform([](auto* ptr) -> auto const& { return *ptr; });
        }
        
    private:
        friend DomTree buildDomTree(ir::Function&);
        
        ir::BasicBlock* _bb = nullptr;
        Node* _parent = nullptr;
        utl::small_vector<Node*> _children;
    };

    struct NodeHash {
        using is_transparent = void;
        size_t operator()(Node const& node) const {
            return (*this)(node.basicBlock());
        }
        size_t operator()(ir::BasicBlock const* bb) const {
            return std::hash<ir::BasicBlock const*>{}(bb);
        }
    };
    
    struct NodeEq {
        using is_transparent = void;
        bool operator()(Node const& a, Node const& b) const {
            return a.basicBlock() == b.basicBlock();
        }
        bool operator()(Node const& a, ir::BasicBlock const* b) const {
            return a.basicBlock() == b;
        }
        bool operator()(ir::BasicBlock const* a, Node const& b) const {
            return a == b.basicBlock();
        }
    };
    
public:
    /// Construct an empty dominator tree.
    DomTree();

    /// \Returns Flat array of nodes in the dominator tree.
    auto nodes() const {
        return _nodes |
               ranges::views::transform([](auto& n) -> Node const& {
                   return n;
               });
    }

    Node const& operator[](ir::BasicBlock const* bb) const {
        auto itr = _nodes.find(bb);
        SC_ASSERT(itr != _nodes.end(), "Not found");
        return *itr;
    }
    
    /// \Returns root of the dominator tree.
    Node const& root() const { return *_root; }

private:
    friend DomTree opt::buildDomTree(ir::Function&);

    Node& findMut(ir::BasicBlock const* bb) {
        return const_cast<Node&>(static_cast<DomTree const*>(this)->operator[](bb));
    }
    
private:
    using NodeSet = utl::hashset<Node, NodeHash, NodeEq>;
    NodeSet _nodes;
    Node* _root;
};

SCATHA(API) void print(DomTree const& domTree);
SCATHA(API) void print(DomTree const& domTree, std::ostream& ostream);

SCATHA(API) utl::hashmap<ir::BasicBlock*, utl::small_vector<ir::BasicBlock*>>
computeDominanceFrontiers(ir::Function& function, DomTree const& domTree);

} // namespace scatha::opt

#endif // SCATHA_OPT_DOMINANCE_H_
