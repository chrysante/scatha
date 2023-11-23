#include "IR/PipelineNodes.h"

#include <range/v3/view.hpp>

using namespace scatha;
using namespace ir;

void PipelineLocalNode::print(std::ostream& str) const { str << pass.name(); }

void PipelineGlobalNode::print(std::ostream& str) const {
    str << pass.name() << "(";
    bool first = true;
    for (auto& node: children) {
        if (!first) {
            str << ", ";
        }
        first = false;
        node->print(str);
    }
    str << ")";
}

void PipelineRoot::print(std::ostream& str) const {
    bool first = true;
    for (auto& node: children) {
        if (!first) {
            str << ", ";
        }
        first = false;
        node->print(str);
    }
}

void PipelineLocalNode::printTree(std::ostream& str,
                                  TreeFormatter& formatter) const {
    str << formatter.beginLine() << pass.name() << std::endl;
}

void PipelineGlobalNode::printTree(std::ostream& str,
                                   TreeFormatter& formatter) const {
    str << formatter.beginLine() << pass.name() << std::endl;
    for (auto [index, node]: children | ranges::views::enumerate) {
        formatter.push(index == children.size() - 1 ? Level::LastChild :
                                                      Level::Child);
        node->printTree(str, formatter);
        formatter.pop();
    }
}

void PipelineRoot::printTree(std::ostream& str,
                             TreeFormatter& formatter) const {
    for (auto [index, node]: children | ranges::views::enumerate) {
        formatter.push(index == children.size() - 1 ? Level::LastChild :
                                                      Level::Child);
        node->printTree(str, formatter);
        formatter.pop();
    }
}
