#ifndef SCATHA_OPT_CALLGRAPH_H_
#define SCATHA_OPT_CALLGRAPH_H_

#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include "Basic/Basic.h"
#include "Common/Graph.h"
#include "IR/Common.h"

namespace scatha::opt {

/// Call graph of functions in a module.
/// Also computes SCC's
///
/// \Warning Direct self recursion is ignored.
class SCATHA_TESTAPI SCCCallGraph {
public:
    class SCCNode;

    /// Node representing a function
    class FunctionNode: public GraphNode<ir::Function*, FunctionNode> {
        using Base = GraphNode<ir::Function*, FunctionNode>;

    public:
        using Base::Base;

        /// \returns the function corresponding to this node
        ir::Function& function() const { return *payload(); }

        /// \returns the SCC this function belongs to
        SCCNode& scc() const { return *_scc; }

        /// \returns the callers of this function. Same as `predecessors()`
        auto callers() const { return predecessors(); }

        /// \returns the callees of this function. Same as `successors()`
        auto callees() const { return successors(); }

        /// \returns all `call` instructions in this function that call \p
        /// callee
        std::span<ir::Call* const> callsites(FunctionNode const& callee) const;

    private:
        friend class SCCCallGraph;

        SCCNode* _scc = nullptr;
        utl::hashmap<FunctionNode const*, utl::small_vector<ir::Call*>>
            _callsites;
    };

    /// Node representing an SCC
    class SCCNode: public GraphNode<void, SCCNode> {
        using Base = GraphNode<void, SCCNode>;

    public:
        using Base::Base;

        /// \returns a view over the function nodes in this SCC
        auto nodes() const {
            return _nodes | ranges::views::transform(
                                [](auto* p) -> auto const& { return *p; });
        }

        /// \returns a view over the functions this SCC
        auto functions() const {
            return _nodes | ranges::views::transform([](auto* node) -> auto& {
                       return node->function();
                   });
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
