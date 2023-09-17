#ifndef SCATHA_CODEGEN_SELECTIONDAG_H_
#define SCATHA_CODEGEN_SELECTIONDAG_H_

#include <iosfwd>
#include <memory>

#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>

#include "Common/Base.h"
#include "Common/Graph.h"
#include "Common/Ranges.h"
#include "IR/Fwd.h"

namespace scatha::cg {

/// Node in the selection DAG
class SCATHA_TESTAPI SelectionNode:
    public GraphNode<ir::Value*, SelectionNode, GraphKind::Directed> {
public:
    using GraphNode::GraphNode;

    /// \Returns the value associated with this node
    ir::Value* value() const { return payload(); }

    /// Nodes of the operands of this instruction
    std::span<SelectionNode* const> operands() { return successors(); }

    /// \overload
    std::span<SelectionNode const* const> operands() const {
        return successors();
    }

    /// Nodes of the users of this instruction
    std::span<SelectionNode* const> users() { return predecessors(); }

    /// \overload
    std::span<SelectionNode const* const> users() const {
        return predecessors();
    }
};

/// Used for instruction selection
class SCATHA_TESTAPI SelectionDAG {
public:
    SelectionDAG() = default;

    /// Builds a selection DAG for the basic block \p BB
    static SelectionDAG build(ir::BasicBlock& BB);

    /// \Returns the basic block this DAG represents
    ir::BasicBlock* basicBlock() const { return BB; }

    /// \Returns the node associated with the instruction \p inst
    /// Traps if no node is found
    SelectionNode* operator[](ir::Instruction* inst) {
        return const_cast<SelectionNode*>(std::as_const(*this)[inst]);
    }

    /// \overload
    SelectionNode const* operator[](ir::Instruction* inst) const;

    /// \Returns a view over the nodes in this DAG
    auto nodes() { return nodemap | ranges::views::values | ToAddress; }

    /// \overload
    auto nodes() const { return nodemap | ranges::views::values | ToAddress; }

private:
    /// Finds the node associated with \p value or creates a new node
    SelectionNode* get(ir::Value* value);

    ir::BasicBlock* BB = nullptr;
    utl::hashmap<ir::Value*, std::unique_ptr<SelectionNode>> nodemap;
};

/// Writes graphviz code representing \p DAG to \p ostream
SCATHA_TESTAPI void generateGraphviz(SelectionDAG const& DAG,
                                     std::ostream& ostream);

} // namespace scatha::cg

#endif // SCATHA_CODEGEN_SELECTIONDAG_H_
