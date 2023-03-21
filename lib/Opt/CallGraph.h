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

class SCATHA(TEST_API) SCCCallGraph {
public:
    class SCCNode;

    /// Node representing a function
    class FunctionNode: public opt::GraphNode<ir::Function*, FunctionNode> {
        using Base = opt::GraphNode<ir::Function*, FunctionNode>;

    public:
        using Base::Base;

        /// \returns the function corresponding to this node
        ir::Function* function() const { return payload(); }

        /// \returns the SCC this function belongs to
        SCCNode* scc() const { return _scc; }

    private:
        friend class SCCCallGraph;

        SCCNode* _scc = nullptr;
    };

    /// Node representing an SCC
    class SCCNode: public opt::GraphNode<void, SCCNode> {
        using Base = opt::GraphNode<void, SCCNode>;

    public:
        using Base::Base;

        /// \returns a view over the function nodes in this SCC
        std::span<FunctionNode const* const> nodes() const { return _nodes; }

        /// \returns a view over the functions this SCC
        auto functions() const {
            return _nodes | ranges::views::transform(
                                [](auto* node) { return node->function(); });
        }

    private:
        friend class SCCCallGraph;

        utl::small_vector<FunctionNode*> _nodes;
    };

public:
    SCCCallGraph()                               = default;
    SCCCallGraph(SCCCallGraph const&)            = delete;
    SCCCallGraph(SCCCallGraph&&)                 = default;
    SCCCallGraph& operator=(SCCCallGraph const&) = delete;
    SCCCallGraph& operator=(SCCCallGraph&&)      = default;

    /// Compute the `SCCCallGraph` of \p mod
    static SCCCallGraph compute(ir::Module& mod);

    /// \returns the node corresponding to \p function
    FunctionNode const& operator[](ir::Function const* function) const {
        auto itr = _functions.find(function);
        SC_ASSERT(itr != _functions.end(), "Not found");
        return *itr;
    }

    /// \returns a view over the SCC's
    std::span<SCCNode const> sccs() const { return _sccs; }

private:
    void computeCallGraph(ir::Module&);

    void computeSCCs();

    FunctionNode& findMut(ir::Function* function) {
        return const_cast<FunctionNode&>(
            (*this)[static_cast<ir::Function const*>(function)]);
    }

private:
    /// We are fine to use a potentially flat hashset here as long as we gather
    /// all the nodes aka functions first and don't modify this afterwards.
    using FuncNodeSet = utl::hashset<FunctionNode,
                                     FunctionNode::PayloadHash,
                                     FunctionNode::PayloadEqual>;

    FuncNodeSet _functions;
    utl::vector<SCCNode> _sccs;
};

} // namespace scatha::opt

#endif // SCATHA_OPT_CALLGRAPH_H_
