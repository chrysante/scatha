#include "Opt/Pipeline/PipelineNodes.h"

#include <range/v3/view.hpp>

using namespace scatha;
using namespace opt;

void PipelineLocalNode::print(std::ostream& str,
                              TreeFormatter& formatter) const {
    str << formatter.beginLine() << pass.name() << std::endl;
}

void PipelineGlobalNode::print(std::ostream& str,
                               TreeFormatter& formatter) const {
    str << formatter.beginLine() << pass.name() << std::endl;
    for (auto [index, node]: children | ranges::views::enumerate) {
        formatter.push(index == children.size() - 1 ? Level::LastChild :
                                                      Level::Child);
        node->print(str, formatter);
        formatter.pop();
    }
}

void PipelineRoot::print(std::ostream& str, TreeFormatter& formatter) const {
    for (auto [index, node]: children | ranges::views::enumerate) {
        formatter.push(index == children.size() - 1 ? Level::LastChild :
                                                      Level::Child);
        node->print(str, formatter);
        formatter.pop();
    }
}
