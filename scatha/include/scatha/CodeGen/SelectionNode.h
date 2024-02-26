#ifndef SCATHA_CG_SELECTIONNODE_H_
#define SCATHA_CG_SELECTIONNODE_H_

#include <span>

#include <range/v3/view.hpp>

#include <scatha/Common/Graph.h>
#include <scatha/Common/List.h>
#include <scatha/Common/Ranges.h>
#include <scatha/IR/Fwd.h>
#include <scatha/MIR/Fwd.h>

namespace scatha::cg {

class SelectionNode;
class SelectionDAG;

using SDValueNodeBase =
    GraphNode<struct SDValueNodeBasePL*, SelectionNode, GraphKind::Directed>;
using SDExecNodeBase =
    GraphNode<struct SDExecNodeBasePL*, SelectionNode, GraphKind::Directed>;

/// Node in the selection DAG, represents one IR instruction
class SCATHA_API SelectionNode: public SDValueNodeBase, public SDExecNodeBase {
public:
    SelectionNode(ir::Instruction const* value);

    SelectionNode(SelectionNode const&) = delete;

    ~SelectionNode();

    /// \Returns the IR instruction associated with this node
    ir::Instruction const* irInst() const { return _irInst; }

    /// \Returns the MIR instruction associated with this node
    List<mir::Instruction> const& mirInstructions() const { return _mirInsts; }

    /// Extracts the instructions of this node
    List<mir::Instruction> extractInstructions();

    /// Set the computed MIR value and list of instructions that compute the
    /// value
    void setSelectedInstructions(List<mir::Instruction> insts);

    /// \Returns `true` if this instruction has been matched, i.e. if
    /// `setSelectedInstructions()` has been called
    bool matched() const { return _matched; }

    /// \Returns a view of the nodes of the operands of this instruction
    std::span<SelectionNode* const> valueDependencies() {
        return SDValueNodeBase::successors();
    }

    /// \overload
    std::span<SelectionNode const* const> valueDependencies() const {
        return SDValueNodeBase::successors();
    }

    /// \Returns a view of the nodes that depent on the value of this node
    std::span<SelectionNode* const> dependentValues() {
        return SDValueNodeBase::predecessors();
    }

    /// \overload
    std::span<SelectionNode const* const> dependentValues() const {
        return SDValueNodeBase::predecessors();
    }

    ///
    void addValueDependency(SelectionNode* node);

    /// \Returns a view of the nodes that must execute _before_ this instruction
    std::span<SelectionNode* const> executionDependencies() {
        return SDExecNodeBase::successors();
    }

    /// \overload
    std::span<SelectionNode const* const> executionDependencies() const {
        return SDExecNodeBase::successors();
    }

    /// \Returns a view of the nodes that must execute _after_ this instruction
    std::span<SelectionNode* const> dependentExecution() {
        return SDExecNodeBase::predecessors();
    }

    /// \overload
    std::span<SelectionNode const* const> dependentExecution() const {
        return SDExecNodeBase::predecessors();
    }

    ///
    void addExecutionDependency(SelectionNode* node);

    ///
    void removeDependency(SelectionNode* node);

    /// Execution dependencies and value dependencies
    auto dependencies() {
        return ranges::views::concat(executionDependencies(),
                                     valueDependencies());
    }

    /// \overlooad
    auto dependencies() const {
        return ranges::views::concat(executionDependencies(),
                                     valueDependencies());
    }

    ///
    auto dependents() {
        return ranges::views::concat(dependentValues(), dependentExecution());
    }

    /// \overload
    auto dependents() const {
        return ranges::views::concat(dependentValues(), dependentExecution());
    }

    /// Merges the dependencies of node \p child into \p this and removes the
    /// edges between \p this and \p child \Warning This function is not
    /// commutative. \p child is not modified and only passed as mutable because
    /// we want mutable access to it's children
    void merge(SelectionNode& child);

    /// Removes all edges from this node and creates edge from each dependent
    /// nodes to all dependencies of this node
    void erase();

private:
    friend class SelectionDAG;

    ir::Instruction const* _irInst = nullptr;
    mir::SSARegister* _register = nullptr; // TODO: Delete this
    List<mir::Instruction> _mirInsts;
    bool _matched = false;
};

} // namespace scatha::cg

#endif // SCATHA_CG_SELECTIONNODE_H_
