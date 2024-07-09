#ifndef SCATHA_OPT_SCCCALLGRAPH_H_
#define SCATHA_OPT_SCCCALLGRAPH_H_

#include <span>
#include <vector>

#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include <scatha/Common/Base.h>
#include <scatha/Common/Graph.h>
#include <scatha/Common/Ranges.h>
#include <scatha/IR/Fwd.h>

namespace scatha::opt {

/// This class represents following graphs for a particular module:
/// - The call graph
/// - The quotient graph of the call graph modulo the equivalence relation
///   induced by the SCCs
///
/// This structure is used for function inlining. The call graph is not
/// necessarily acyclic, but the quotient graph obtained from the SSCs is
/// guaranteed to be DAG. We need the acyclic property for the inlining
/// algorithm.
///
/// \Warning Direct self recursion is ignored.
class SCATHA_API SCCCallGraph {
public:
    class SCCNode;

    /// Node representing a function
    class FunctionNode:
        public GraphNode<ir::Function*, FunctionNode, GraphKind::Directed> {
        using Base =
            GraphNode<ir::Function*, FunctionNode, GraphKind::Directed>;

    public:
        using Base::Base;

        /// \returns the function corresponding to this node
        ir::Function& function() const { return *payload(); }

        /// \returns the SCC this function belongs to
        SCCNode& scc() const { return *_scc; }

        /// \returns the callers of this function. Same as `predecessors()`
        std::span<FunctionNode* const> callers() { return predecessors(); }

        /// \overload
        std::span<FunctionNode const* const> callers() const {
            return predecessors();
        }

        /// \returns the callees of this function. Same as `successors()`
        std::span<FunctionNode* const> callees() { return successors(); }

        /// \overload
        std::span<FunctionNode const* const> callees() const {
            return successors();
        }

        /// \returns all `call` instructions in this function that call
        /// \p callee
        utl::hashset<ir::Call*> const& callsites(
            FunctionNode const& callee) const;

    private:
        friend class SCCCallGraph;

        utl::hashset<ir::Call*>& mutCallsites(FunctionNode const& callee) {
            return const_cast<utl::hashset<ir::Call*>&>(
                static_cast<FunctionNode const*>(this)->callsites(callee));
        }

        /// The SCC this node belongs to
        SCCNode* _scc = nullptr;

        /// For each successor we store a list of all `call` instructions in
        /// this function that call that successor function. This is necessary
        /// because a particular function could be called multiple times from
        /// one other function and we want to store that information.
        utl::hashmap<FunctionNode const*, utl::hashset<ir::Call*>> _callsites;
    };

    /// Node representing an SCC
    class SCCNode: public GraphNode<void, SCCNode, GraphKind::Directed> {
        using Base = GraphNode<void, SCCNode, GraphKind::Directed>;

    public:
        using Base::Base;

        explicit SCCNode(utl::small_vector<FunctionNode*> nodes);

        /// \returns a view over the function nodes in this SCC
        std::span<FunctionNode* const> nodes() { return _nodes; }

        /// \overload
        std::span<FunctionNode const* const> nodes() const { return _nodes; }

        /// \returns a view over the functions this SCC
        auto functions() const {
            return _nodes | ranges::views::transform([](auto* node) -> auto& {
                return node->function();
            });
        }

        void addNode(FunctionNode* node) { _nodes.push_back(node); }

    private:
        friend class SCCCallGraph;

        /// The functions in this SCC
        utl::small_vector<FunctionNode*> _nodes;
    };

    struct Modification {
        enum Type {
            None,        /// No structural change to the call graph
            RemovedEdge, /// A call edge has been removed, but no SCC was split
            SplitSCC,    /// A call edge has been removed, resulting in an SCC
                         /// being split
            AddedEdge, /// A call edge has been added, but no new SCC was formed
            MergedSCCs /// The callers SCC has been split
        };

        Modification(Type type = None): type(type) {}

        Modification(Type type, utl::small_vector<SCCNode*, 2> modifiedSCCs):
            type(type), modifiedSCCs(std::move(modifiedSCCs)) {}

        Type type;
        utl::small_vector<SCCNode*, 2> modifiedSCCs;
    };

    explicit SCCCallGraph(ir::Module& mod): mod(&mod) {}
    SCCCallGraph(SCCCallGraph const&) = delete;
    SCCCallGraph(SCCCallGraph&&) = default;
    SCCCallGraph& operator=(SCCCallGraph const&) = delete;
    SCCCallGraph& operator=(SCCCallGraph&&) = default;

    /// Compute the `SCCCallGraph` of \p mod
    static SCCCallGraph compute(ir::Module& mod);

    /// Compute the call graph of \p mod without computing the SCCs.
    static SCCCallGraph computeNoSCCs(ir::Module& mod);

    /// \returns the node corresponding to \p function
    FunctionNode& operator[](ir::Function const* function) {
        return const_cast<FunctionNode&>(
            static_cast<SCCCallGraph const&>(*this)[function]);
    }

    /// \overload
    FunctionNode const& operator[](ir::Function const* function) const {
        auto itr = _funcMap.find(function);
        SC_ASSERT(itr != _funcMap.end(), "Not found");
        return *itr->second;
    }

    /// \returns a view over the SCC's
    auto sccs() { return _sccs | Dereference; }

    /// \overload
    auto sccs() const { return _sccs | Dereference; }

    /// Remove the call instruction \p callInst from the call graph
    /// We explicitly pass in the caller and the callee, because the call
    /// instruction is already deallocated when this function is called.
    Modification removeCall(ir::Function* caller, ir::Function const* callee,
                            ir::Call const* callInst);

    ///
    Modification addCall(ir::Function* caller, ir::Function* callee,
                         ir::Call* callInst);

    /// Result structure for `recomputeCallees()`
    struct RecomputeCalleesResult {
        /// New callees found. New callees can be found because dynamically
        /// dispatch calls have been devirtualized
        utl::hashset<FunctionNode*> newCallees;

        /// SSCs can be modified either because calls got removed and SSCs have
        /// been split or because a newly discovered call edge introduced a new
        /// SCC
        utl::hashset<SCCNode*> modifiedSCCs;

        /// \Returns `true` if either `newCallees` or `modifiedSCCs` is not
        /// empty
        explicit operator bool() const {
            return !newCallees.empty() || !modifiedSCCs.empty();
        }

        /// Merges `newCallees` and `modifiedSCCs` from \p rhs
        void merge(RecomputeCalleesResult const& rhs);
    };

    /// Called after applying location transformations to a function. This
    /// revalidates the callgraph, so we can catch new inlining candidates,
    /// e.g., devirtualized calls
    RecomputeCalleesResult recomputeCallees(FunctionNode& node);

    /// Checks if the call graph still correctly represents the structure of the
    /// module and traps if errors are found
    void validate() const;

    /// Updates the function of the node
    /// It's neceessary to have this cumbersome interface because the function
    /// nodes are hashed via their function pointers, so we extract the node and
    /// insert it again
    void updateFunctionPointer(FunctionNode* node, ir::Function* newFunction);

private:
    void computeCallGraph();

    void computeSCCs();

    /// Used to recompute edges for SCCs created by splitting another SCC
    void recomputeForwardEdges(SCCNode* scc);

    void recomputeBackEdges(SCCNode* oldSCC, std::span<SCCNode* const> newSCCs);

    FunctionNode& findMut(ir::Function const* function) {
        return const_cast<FunctionNode&>((*this)[function]);
    }

    /// The module whose call graph is represented
    ir::Module* mod = nullptr;

    /// List of function nodes
    std::vector<std::unique_ptr<FunctionNode>> _nodes;

    /// Maps functions to function nodes
    utl::hashmap<ir::Function const*, FunctionNode*> _funcMap;

    /// List of SCCs
    std::vector<std::unique_ptr<SCCNode>> _sccs;
};

/// Writes graphviz code representing \p graph to \p ostream
SCATHA_API void generateGraphviz(SCCCallGraph const& graph,
                                 std::ostream& ostream);

/// Debug utility to generate graphviz representation of the graph to a
/// temporary file
SCATHA_API void generateGraphvizTmp(SCCCallGraph const& graph);

} // namespace scatha::opt

#endif // SCATHA_OPT_SCCCALLGRAPH_H_
