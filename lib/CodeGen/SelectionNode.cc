#include "CodeGen/SelectionNode.h"

#include "MIR/Instruction.h"

using namespace scatha;
using namespace cg;

SelectionNode::SelectionNode(ir::Instruction const* inst): _irInst(inst) {}

SelectionNode::~SelectionNode() = default;

void SelectionNode::setMIR(mir::SSARegister* reg,
                           List<mir::Instruction> insts) {
    _register = reg;
    _mirInsts = std::move(insts);
    _matched = true;
}

void SelectionNode::addValueDependency(SelectionNode* node) {
    if (node == this) {
        return;
    }
    SDValueNodeBase::addSuccessor(node);
    node->SDValueNodeBase::addPredecessor(this);
}

void SelectionNode::addExecutionDependency(SelectionNode* node) {
    if (node == this) {
        return;
    }
    SDExecNodeBase::addSuccessor(node);
    node->SDExecNodeBase::addPredecessor(this);
}

void SelectionNode::removeDependency(SelectionNode* node) {
    SDValueNodeBase::removeSuccessor(node);
    SDExecNodeBase::removeSuccessor(node);
    node->SDValueNodeBase::removePredecessor(this);
    node->SDExecNodeBase::removePredecessor(this);
}

static void copyValueDependencies(SelectionNode& oldDepent,
                                  SelectionNode& newDepent) {
    for (auto* dependency: oldDepent.valueDependencies() | ToSmallVector<>) {
        newDepent.addValueDependency(dependency);
    }
}

static void copyValueDependents(SelectionNode& oldDepency,
                                SelectionNode& newDepency) {
    for (auto* dependent: oldDepency.dependentValues() | ToSmallVector<>) {
        dependent->addValueDependency(&newDepency);
    }
}

static void copyExecutionDependencies(SelectionNode& oldDepent,
                                      SelectionNode& newDepent) {
    for (auto* dependency: oldDepent.executionDependencies() | ToSmallVector<>)
    {
        newDepent.addExecutionDependency(dependency);
    }
}

static void copyExecutionDependents(SelectionNode& oldDepency,
                                    SelectionNode& newDepency) {
    for (auto* dependent: oldDepency.dependentExecution() | ToSmallVector<>) {
        dependent->addExecutionDependency(&newDepency);
    }
}

void SelectionNode::merge(SelectionNode& child) {
    copyValueDependencies(child, *this);
    copyValueDependents(child, *this);
    copyExecutionDependencies(child, *this);
    copyExecutionDependents(child, *this);
    removeDependency(&child);
}

void SelectionNode::erase() {
    SC_ASSERT(dependentValues().empty(),
              "We can't erase this node if other values depend on it");
    for (auto* dependent: dependentExecution()) {
        for (auto* dependency: executionDependencies()) {
            dependent->addExecutionDependency(dependency);
        }
    }
    for (auto* dependent: dependents()) {
        dependent->removeDependency(this);
    }
    for (auto* dependency: dependencies()) {
        this->removeDependency(dependency);
    }
}
