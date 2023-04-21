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

class SCATHA_TESTAPI InterferenceGraph {
    template <typename N>
    auto getNodeView() const {
        return nodes | ranges::views::transform(
                           [](auto& ptr) -> N* { return ptr.get(); });
    }

public:
    class Node: public GraphNode<void, Node, GraphKind::Undirected> {
    public:
        explicit Node(mir::Register* reg): _reg(reg) {}

        size_t color() const { return col; }

        mir::Register* reg() const { return _reg; }

    private:
        friend class InterferenceGraph;

        size_t col = ~size_t{ 0 };
        mir::Register* _reg;
    };

    static InterferenceGraph compute(mir::Function& F);

    void colorize();

    size_t numColors() const { return numCols; }

    auto begin() const { return getNodeView<Node const>().begin(); }

    auto end() const { return getNodeView<Node const>().end(); }

    size_t size() const { return nodes.size(); }

    bool empty() const { return nodes.empty(); }

private:
    void computeImpl(mir::Function&);
    void addRegister(mir::Register*);
    void addEdges(mir::Register*, auto&& listOfRegs);
    Node* find(mir::Register*);

    utl::hashmap<mir::Register*, Node*> regMap;
    utl::vector<std::unique_ptr<Node>> nodes;
    size_t numCols = 0;
};

} // namespace scatha::cg

#endif // SCATHA_CODEGEN_INTERFERENCEGRAPH_H_
