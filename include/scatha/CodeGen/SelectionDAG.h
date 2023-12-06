#ifndef SCATHA_CODEGEN_SELECTIONDAG_H_
#define SCATHA_CODEGEN_SELECTIONDAG_H_

#include <iosfwd>
#include <memory>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>
#include <utl/utility.hpp>

#include <scatha/CodeGen/SelectionNode.h>
#include <scatha/Common/Base.h>
#include <scatha/Common/Ranges.h>
#include <scatha/IR/Fwd.h>

namespace scatha::cg {

/// Basic block representation used for instruction selection
class SCATHA_API SelectionDAG {
public:
    SelectionDAG() = default;

    /// Builds a selection DAG for the basic block \p BB
    static SelectionDAG Build(ir::BasicBlock const& BB);

    /// ## Views

    /// \Returns a view over all nodes in this DAG
    std::span<SelectionNode* const> nodes() {
        return std::span{ all.values() };
    }

    /// \overload
    std::span<SelectionNode const* const> nodes() const {
        return std::span{ all.values() };
    }

    /// \Returns a view over all node whose values are used by other basic
    /// blocks
    std::span<SelectionNode* const> outputNodes() const {
        return outputs.values();
    }

    /// \Returns the root node of this DAG, i.e. the node corresponding to the
    /// terminator
    SelectionNode* root() {
        return const_cast<SelectionNode*>(
            static_cast<SelectionDAG const*>(this)->root());
    }

    /// \overload
    SelectionNode const* root() const;

    /// ## Queries

    /// \Returns the basic block this DAG represents
    ir::BasicBlock const* basicBlock() const { return BB; }

    /// \Returns the node associated with the instruction \p inst or null if no
    /// node is found
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
        return outputs.contains(node);
    }

    /// \Returns the set of all nodes which have (transitive) dependencies on
    ///  \p node
    utl::hashset<SelectionNode const*> const& dependencies(
        SelectionNode const* node) {
        return deps[node];
    }

    /// \Returns a list of all nodes in this DAG in topsort order. To determine
    /// the order all dependencies (value and execution) are considered
    utl::small_vector<SelectionNode*> topsort();

    /// # Modifiers

    ///
    void erase(SelectionNode* node);

private:
    /// Finds the node associated with \p value or creates a new node
    SelectionNode* get(ir::Instruction const* value);

    /// The represented block
    ir::BasicBlock const* BB = nullptr;

    /// This map has an entry for every node in the DAG
    utl::hashmap<ir::Value const*, std::unique_ptr<SelectionNode>> map;

    /// Maps each node to the set of nodes whose execution depends on the key
    /// node
    utl::hashmap<SelectionNode const*, utl::hashset<SelectionNode const*>> deps;

    /// Set of all nodes
    utl::hashset<SelectionNode*> all;

    /// Set of all nodes with side effects
    utl::hashset<SelectionNode*> sideEffects;

    /// Set of all output nodes of this block
    utl::hashset<SelectionNode*> outputs;
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
