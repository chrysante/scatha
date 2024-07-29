#include "IR/PipelineNodes.h"

#include <range/v3/view.hpp>

using namespace scatha;
using namespace ir;
using namespace ranges::views;

void PipelineFunctionNode::print(std::ostream& str) const {
    str << pass.name();
}

bool PipelineModuleNode::execute(ir::Context& ctx, ir::Module& mod) const {
    auto local = [&]() -> FunctionPass {
        if (children.empty()) {
            return {};
        }
        return FunctionPass([this](ir::Context& ctx, ir::Function& F) {
            bool result = false;
            for (auto& child: children) {
                result |= child->execute(ctx, F);
            }
            return result;
        });
    }();
    return pass(ctx, mod, std::move(local));
}

void PipelineModuleNode::print(std::ostream& str) const {
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

void PipelineFunctionNode::printTree(std::ostream& str,
                                     TreeFormatter& formatter) const {
    str << formatter.beginLine() << pass.name() << std::endl;
}

void PipelineModuleNode::printTree(std::ostream& str,
                                   TreeFormatter& formatter) const {
    str << formatter.beginLine() << pass.name() << std::endl;
    for (auto [index, node]: children | enumerate) {
        formatter.push(index == children.size() - 1 ? Level::LastChild :
                                                      Level::Child);
        node->printTree(str, formatter);
        formatter.pop();
    }
}

void PipelineRoot::printTree(std::ostream& str,
                             TreeFormatter& formatter) const {
    for (auto [index, node]: children | enumerate) {
        formatter.push(index == children.size() - 1 ? Level::LastChild :
                                                      Level::Child);
        node->printTree(str, formatter);
        formatter.pop();
    }
}
