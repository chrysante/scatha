#ifndef SCATHA_OPT_DOMTREE_H_
#define SCATHA_OPT_DOMTREE_H_

#include <iosfwd>
#include <span>

#include <utl/vector.hpp>

#include "Basic/Basic.h"
#include "IR/Common.h"

namespace scatha::opt {

class DomTree;

void print(DomTree const& domTree);
void print(DomTree const& domTree, std::ostream& ostream);

DomTree buildDomTree(ir::Function& function);

class DomTree {
public:
    struct Node {
        ir::BasicBlock* basicBlock;
        utl::small_vector<Node*> children;
    };
    
public:
    /// Construct an empty dominator tree.
    DomTree();
    
    /// Flat array of nodes in the dominator tree.
    std::span<Node const> nodes() const { return _nodes; }
    
private:
    friend DomTree opt::buildDomTree(ir::Function& function);
    
private:
    utl::vector<Node> _nodes;
};

} // namespace scatha::opt

#endif // SCATHA_OPT_DOMTREE_H_
