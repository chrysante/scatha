#include "IR/PipelineNodes.h"

#include <range/v3/view.hpp>

using namespace scatha;
using namespace ir;
using namespace ranges::views;

bool PipelineLoopNode::execute(ir::Context& ctx, ir::LNFNode& loop) const {
    return pass(ctx, loop);
}

void PipelineLoopNode::print(std::ostream&) const {}

void PipelineLoopNode::printTree(std::ostream&, TreeFormatter&) const {}

void PipelineFunctionNode::print(std::ostream& str) const {
    str << pass.name();
}

bool PipelineFunctionNode::execute(ir::Context& ctx, ir::Function& F) const {
    auto loopPass = [&]() -> LoopPass {
        if (children.empty()) {
            return {};
        }
        return LoopPass([this](ir::Context& ctx, ir::LNFNode& loop) {
            bool result = false;
            for (auto& child: children) {
                result |= child->execute(ctx, loop);
            }
            return result;
        });
    }();
    return pass(ctx, F, loopPass);
}

bool PipelineModuleNode::execute(ir::Context& ctx, ir::Module& mod) const {
    auto fnPass = [&]() -> FunctionPass {
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
    return pass(ctx, mod, fnPass);
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
