#ifndef SCATHA_CODEGEN_SELECTIONDAG_H_
#define SCATHA_CODEGEN_SELECTIONDAG_H_

#include <iosfwd>
#include <memory>

#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>
#include <utl/utility.hpp>

#include <scatha/Common/Allocator.h>
#include <scatha/Common/Base.h>
#include <scatha/Common/Graph.h>
#include <scatha/Common/Ranges.h>
#include <scatha/IR/Fwd.h>

namespace scatha::cg {

class SelectionDAG;

/// Node in the selection DAG
class SCATHA_API SelectionNode:
    public GraphNode<ir::Value const*, SelectionNode, GraphKind::Directed> {
public:
    SelectionNode(ir::Value const* value): GraphNode(value) {}

    /// \Returns the value associated with this node
    ir::Value const* value() const { return payload(); }

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
class SCATHA_API SelectionDAG {
public:
    SelectionDAG() = default;

    /// Builds a selection DAG for the basic block \p BB
    static SelectionDAG Build(ir::BasicBlock const& BB);

    /// \Returns the basic block this DAG represents
    ir::BasicBlock const* basicBlock() const { return BB; }

    /// \Returns the node associated with the instruction \p inst
    /// Traps if no node is found
    SelectionNode* operator[](ir::Instruction const* inst) {
        return const_cast<SelectionNode*>(std::as_const(*this)[inst]);
    }

    /// \overload
    SelectionNode const* operator[](ir::Instruction const* inst) const;

    /// \Returns a view over the nodes in this DAG
    auto nodes() { return nodemap | ranges::views::values | ToAddress; }

    /// \overload
    auto nodes() const { return nodemap | ranges::views::values | ToAddress; }

    std::span<ir::Instruction const* const> criticalInstructions() const {
        return critical;
    }

private:
    /// Finds the node associated with \p value or creates a new node
    SelectionNode* get(ir::Value const* value);

    ir::BasicBlock const* BB = nullptr;
    utl::vector<ir::Instruction const*> critical;
    utl::hashmap<ir::Value const*, SelectionNode*> nodemap;
    MonotonicBufferAllocator allocator;
};

/// Writes graphviz code representing \p DAG to \p ostream
SCATHA_API void generateGraphviz(SelectionDAG const& DAG,
                                 std::ostream& ostream);

/// Debug utility to generate graphviz representation of the DAG to a temporary
/// file
SCATHA_API void generateGraphvizTmp(SelectionDAG const& DAG);

} // namespace scatha::cg

#endif // SCATHA_CODEGEN_SELECTIONDAG_H_
