#ifndef SCATHA_CODEGEN_SELECTIONDAG_H_
#define SCATHA_CODEGEN_SELECTIONDAG_H_

#include <iosfwd>
#include <memory>

#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>
#include <utl/utility.hpp>

#include "Common/Allocator.h"
#include "Common/Base.h"
#include "Common/Graph.h"
#include "Common/Ranges.h"
#include "IR/Fwd.h"

namespace scatha::cg {

class SelectionDAG;

/// Node in the selection DAG
class SCATHA_TESTAPI SelectionNode:
    public GraphNode<ir::Value*, SelectionNode, GraphKind::Directed> {
public:
    SelectionNode(ir::Value* value): GraphNode(value) {}

    /// \Returns the value associated with this node
    ir::Value* value() const { return payload(); }

    /// \Returns a view of the nodes of the operands of this instruction
    std::span<SelectionNode* const> operands() { return successors(); }

    /// \overload
    std::span<SelectionNode const* const> operands() const {
        return successors();
    }

    /// \Returns a view of the nodes of the users of this instruction
    std::span<SelectionNode* const> users() { return predecessors(); }

    /// \overload
    std::span<SelectionNode const* const> users() const {
        return predecessors();
    }

    /// \Returns the position of this instruction in the basic block
    ssize_t index() const { return _index; }

private:
    friend class SelectionDAG;
    void setIndex(ssize_t index) { _index = utl::narrow_cast<int32_t>(index); }

    int32_t _index = 0;
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
    utl::hashmap<ir::Value*, SelectionNode*> nodemap;
    MonotonicBufferAllocator allocator;
};

/// Writes graphviz code representing \p DAG to \p ostream
SCATHA_TESTAPI void generateGraphviz(SelectionDAG const& DAG,
                                     std::ostream& ostream);

/// Debug utility to generate graphviz representation of the DAG to a temporary
/// file
SCATHA_TESTAPI void generateGraphvizTmp(SelectionDAG const& DAG);

} // namespace scatha::cg

#endif // SCATHA_CODEGEN_SELECTIONDAG_H_
