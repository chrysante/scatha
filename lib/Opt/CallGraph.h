#ifndef SCATHA_OPT_CALLGRAPH_H_
#define SCATHA_OPT_CALLGRAPH_H_

#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include "Basic/Basic.h"
#include "Opt/Graph.h"

namespace scatha::ir {

class Function;
class Module;

} // namespace scatha::ir

namespace scatha::opt {

class CallGraph {
public:
    class Node: public opt::GraphNode<ir::Function*, Node> {
        using Base = opt::GraphNode<ir::Function*, Node>;

    public:
        using Base::Base;

        ir::Function* function() const { return payload(); }
    };

public:
    static CallGraph build(ir::Module& mod);

    Node const& operator[](ir::Function const* function) const {
        auto itr = _nodes.find(function);
        SC_ASSERT(itr != _nodes.end(), "Not found");
        return *itr;
    }

private:
    Node& findMut(ir::Function* function) {
        return const_cast<Node&>(
            (*this)[static_cast<ir::Function const*>(function)]);
    }

private:
    using NodeSet = utl::hashset<Node, Node::PayloadHash, Node::PayloadEqual>;
    NodeSet _nodes;
};

} // namespace scatha::opt

#endif // SCATHA_OPT_CALLGRAPH_H_
