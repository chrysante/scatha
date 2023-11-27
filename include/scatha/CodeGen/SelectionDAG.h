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

    /// Sets the 'matched' flag to \p value
    void setMatched(bool value = true) { _matched = value; }
    
    /// \Returns `true` if the 'matched' flag has been set
    bool matched() const { return _matched; }

private:
    friend class SelectionDAG;
    void setIndex(ssize_t index) { _index = utl::narrow_cast<int32_t>(index); }

    int32_t _index = 0;
    bool _matched = false;
};

/// Used for instruction selection
class SCATHA_API SelectionDAG {
public:
    SelectionDAG() = default;

    /// Builds a selection DAG for the basic block \p BB
    static SelectionDAG Build(ir::BasicBlock const& BB);

    /// ## Views

    /// \Returns a view over all nodes in this DAG
    auto nodes() const {
        return std::span{ map.values() } | ranges::views::values;
    }

    /// \Returns a view over all node with side effects in their relative order
    std::span<SelectionNode* const> sideEffectNodes() const {
        return orderedSideEffects;
    }
    
    /// \Returns a view over all node whose values are used by other basic blocks
    std::span<SelectionNode* const> outputNodes() const {
        return outputs.values();
    }

    /// ## Queries

    /// \Returns the basic block this DAG represents
    ir::BasicBlock const* basicBlock() const { return BB; }

    /// \Returns the node associated with the instruction \p inst
    /// Traps if no node is found
    SelectionNode* operator[](ir::Instruction const* inst) {
        return const_cast<SelectionNode*>(std::as_const(*this)[inst]);
    }

    /// \overload
    SelectionNode const* operator[](ir::Instruction const* inst) const;

    /// \Returns `true` if the instruction of \p node has visible side effects
    bool hasSideEffects(SelectionNode const* node) const {
        return sideEffects.contains(node);
    }

    /// \Returns `true` if \p node is an output of this block
    bool isOutput(SelectionNode const* node) const {
        return sideEffects.contains(node);
    }

private:
    /// Finds the node associated with \p value or creates a new node
    SelectionNode* get(ir::Value const* value);

    /// The represented block
    ir::BasicBlock const* BB = nullptr;

    /// This map has an entry for every node in the DAG
    utl::hashmap<ir::Value const*, SelectionNode*> map;

    /// Set of all nodes with side effects
    utl::hashset<SelectionNode*> sideEffects;

    /// List of all nodes with side effects
    utl::small_vector<SelectionNode*> orderedSideEffects;

    /// Set of all output nodes of this block
    utl::hashset<SelectionNode*> outputs;

    /// Allocator used to allocate the nodes
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
