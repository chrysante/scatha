#ifndef SCATHA_CG_INTERFERENCEGRAPH_H_
#define SCATHA_CG_INTERFERENCEGRAPH_H_

#include <memory>
#include <span>

#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include "Basic/Basic.h"
#include "Common/Graph.h"
#include "IR/Common.h"

namespace scatha::cg {

class InterferenceGraph {
    auto getNodeView() const {
        return nodes | ranges::views::transform(
                           [](auto& ptr) -> auto const* { return ptr.get(); });
    }

public:
    class Node: public GraphNode<void, Node, GraphKind::Undirected> {
    public:
        explicit Node(ir::Value const* value): vals({ value }) {}

        int color() const { return col; }

        std::span<ir::Value const* const> values() const { return vals; }

    private:
        friend class InterferenceGraph;

        int col = -1;
        utl::small_vector<ir::Value const*> vals;
    };

    SCATHA_TESTAPI static InterferenceGraph compute(
        ir::Function const& function);

    auto begin() const { return getNodeView().begin(); }

    auto end() const { return getNodeView().end(); }

    size_t size() const { return nodes.size(); }

    bool empty() const { return nodes.empty(); }

private:
    void computeImpl(ir::Function const&);
    void addValue(ir::Value const*);
    void addEdges(ir::Value const*, auto&& listOfValues);
    Node* find(ir::Value const*);

    utl::hashmap<ir::Value const*, Node*> valueMap;
    utl::vector<std::unique_ptr<Node>> nodes;
};

} // namespace scatha::cg

#endif // SCATHA_CG_INTERFERENCEGRAPH_H_
