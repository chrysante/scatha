#ifndef SCATHA_CODEGEN_SELECTIONDAG_H_
#define SCATHA_CODEGEN_SELECTIONDAG_H_

#include <iosfwd>
#include <memory>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>
#include <utl/utility.hpp>

#include <scatha/Common/Allocator.h>
#include <scatha/Common/Base.h>
#include <scatha/Common/Graph.h>
#include <scatha/Common/List.h>
#include <scatha/Common/Ranges.h>
#include <scatha/IR/Fwd.h>
#include <scatha/MIR/Fwd.h>

namespace scatha::cg {

class SelectionDAG;

/// Node in the selection DAG
class SCATHA_API SelectionNode:
    public GraphNode<void, SelectionNode, GraphKind::Directed> {
public:
    SelectionNode(ir::Value const* value): _irValue(value) {}

    SelectionNode(SelectionNode const&) = delete;

    ~SelectionNode();

    /// \Returns the IR value associated with this node
    ir::Value const* irValue() const { return _irValue; }

    /// \Returns the MIR value associated with this node
    mir::Value* mirValue() const { return _mirValue; }

    /// \Returns the MIR instruction associated with this node
    List<mir::Instruction> const& mirInstructions() const { return _mirInsts; }

    /// Set the computed MIR value and list of instructions that compute the
    /// value
    void setMIR(mir::Value* value, List<mir::Instruction> insts);

    /// \Returns `true` if this instruction has been matched, i.e. if `setMIR()`
    /// has been called
    bool matched() const { return _matched; }

    /// \Returns a view of the nodes of the operands of this instruction
    std::span<SelectionNode* const> valueDependencies() { return valueDeps; }

    /// \overload
    std::span<SelectionNode const* const> valueDependencies() const {
        return valueDeps;
    }

    void removeDependency(SelectionNode const* node) {
        valueDeps.erase(ranges::remove(valueDeps, node), valueDeps.end());
        removeSuccessor(node);
    }

    ///
    void addValueDependency(SelectionNode* node) {
        if (!ranges::contains(valueDeps, node)) {
            valueDeps.push_back(node);
        }
    }

    /// \Returns a view of the nodes that must execute before this instruction
    std::span<SelectionNode* const> executionDependencies() {
        return successors();
    }

    /// \overload
    std::span<SelectionNode const* const> executionDependencies() const {
        return successors();
    }

    ///
    void addExecutionDependency(SelectionNode* node) { addSuccessor(node); }

private:
    friend class SelectionDAG;

    ir::Value const* _irValue = nullptr;
    mir::Value* _mirValue = nullptr;
    List<mir::Instruction> _mirInsts;
    utl::small_vector<SelectionNode*, 3> valueDeps;
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

    /// \Returns a view over all node whose values are used by other basic
    /// blocks
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
    //    utl::small_vector<SelectionNode*> orderedSideEffects;

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
SCATHA_API void generateGraphvizTmp(SelectionDAG const& DAG, std::string name);

/// \overload
SCATHA_API void generateGraphvizTmp(SelectionDAG const& DAG);

} // namespace scatha::cg

#endif // SCATHA_CODEGEN_SELECTIONDAG_H_
