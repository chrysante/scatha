#ifndef SCATHA_IR_PIPELINENODES_H_
#define SCATHA_IR_PIPELINENODES_H_

#include <iosfwd>
#include <memory>

#include <utl/vector.hpp>

#include "Common/TreeFormatter.h"
#include "IR/Pass.h"

namespace scatha::ir {

/// Represents a loop pass in the pipeline tree
class PipelineLoopNode {
public:
    explicit PipelineLoopNode(LoopPass pass): pass(std::move(pass)) {}

    bool execute(ir::Context& ctx, ir::LNFNode& loop) const;

    void print(std::ostream&) const;

    void printTree(std::ostream&, TreeFormatter&) const;

private:
    LoopPass pass;
};

/// Represents a function pass in the pipeline tree
class PipelineFunctionNode {
public:
    explicit PipelineFunctionNode(
        FunctionPass pass,
        utl::small_vector<std::unique_ptr<PipelineLoopNode>> children = {}):
        pass(std::move(pass)), children(std::move(children)) {}

    bool execute(ir::Context& ctx, ir::Function& F) const;

    void print(std::ostream&) const;

    void printTree(std::ostream&, TreeFormatter&) const;

private:
    FunctionPass pass;
    utl::small_vector<std::unique_ptr<PipelineLoopNode>> children;
};

/// Represents a module pass in the pipeline tree
class PipelineModuleNode {
public:
    PipelineModuleNode(
        ModulePass pass,
        utl::small_vector<std::unique_ptr<PipelineFunctionNode>> children = {}):
        pass(std::move(pass)), children(std::move(children)) {}

    PipelineModuleNode(ModulePass pass,
                       std::unique_ptr<PipelineFunctionNode> singleChild):
        pass(std::move(pass)) {
        children.push_back(std::move(singleChild));
    }

    bool execute(ir::Context& ctx, ir::Module& mod) const;

    void print(std::ostream&) const;

    void printTree(std::ostream&, TreeFormatter&) const;

private:
    ModulePass pass;
    utl::small_vector<std::unique_ptr<PipelineFunctionNode>> children;
};

/// Represents the root of the pipeline tree
class PipelineRoot {
public:
    PipelineRoot() = default;

    explicit PipelineRoot(std::unique_ptr<PipelineModuleNode> singleChild) {
        children.push_back(std::move(singleChild));
    }

    explicit PipelineRoot(
        utl::small_vector<std::unique_ptr<PipelineModuleNode>> children):
        children(std::move(children)) {}

    bool execute(ir::Context& ctx, ir::Module& mod) const {
        bool result = false;
        for (auto& child: children) {
            result |= child->execute(ctx, mod);
        }
        return result;
    }

    void print(std::ostream&) const;

    void printTree(std::ostream&, TreeFormatter&) const;

    bool empty() const { return children.empty(); }

private:
    utl::small_vector<std::unique_ptr<PipelineModuleNode>> children;
};

} // namespace scatha::ir

#endif // SCATHA_IR_PIPELINENODES_H_
