#ifndef SCATHA_CODEGEN_INTERFERENCEGRAPH_H_
#define SCATHA_CODEGEN_INTERFERENCEGRAPH_H_

#include <memory>
#include <span>

#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include "Common/Base.h"
#include "Common/Graph.h"
#include "MIR/Fwd.h"

namespace scatha::cg {

/// Used for register allocation
class SCTEST_API InterferenceGraph {
    template <typename N>
    auto getNodeView() const {
        return nodes | ranges::views::transform(
                           [](auto& ptr) -> N* { return ptr.get(); });
    }

public:
    /// Represents a node in the interference graph
    class Node: public GraphNode<void, Node, GraphKind::Undirected> {
    public:
        /// Construct a node from a register
        explicit Node(mir::Register* reg): _reg(reg) {}

        /// \Returns the color index associated with this node
        size_t color() const { return col; }

        /// \Returns the register associated with this node
        mir::Register* reg() const { return _reg; }

    private:
        friend class InterferenceGraph;

        size_t col = ~size_t{ 0 };
        mir::Register* _reg;
    };

    /// Static constructor
    static InterferenceGraph compute(mir::Function& F);

    /// Colorizes the graph
    void colorize();

    /// \Returns the number of colors used to colorize the graph
    size_t numColors() const { return numCols; }

    /// \Returns the begin iterator to the range of nodes
    auto begin() const { return getNodeView<Node const>().begin(); }

    /// \Returns the end iterator to the range of nodes
    auto end() const { return getNodeView<Node const>().end(); }

    /// \Returns the number of nodes in this graph
    size_t size() const { return nodes.size(); }

    /// \Returns `true` if this graph has no nodes
    bool empty() const { return nodes.empty(); }

    /// \Returns the function this interference graph represents
    mir::Function* function() const { return F; }

private:
    void computeImpl(mir::Function&);
    void addRegister(mir::Register*);
    void addEdges(mir::Register*, auto&& listOfRegs);
    Node* find(mir::Register*);

    mir::Function* F = nullptr;
    utl::hashmap<mir::Register*, Node*> regMap;
    utl::vector<std::unique_ptr<Node>> nodes;
    size_t numCols = 0;
};

/// Writes graphviz code representing \p graph to \p ostream
SCATHA_API void generateGraphviz(InterferenceGraph const& graph,
                                 std::ostream& ostream);

/// Debug utility to generate graphviz representation of the graph to a
/// temporary file
SCATHA_API void generateGraphvizTmp(InterferenceGraph const& graph);

} // namespace scatha::cg

#endif // SCATHA_CODEGEN_INTERFERENCEGRAPH_H_
