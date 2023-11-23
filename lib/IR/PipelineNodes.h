#ifndef SCATHA_IR_PIPELINENODES_H_
#define SCATHA_IR_PIPELINENODES_H_

#include <iosfwd>
#include <memory>

#include <utl/vector.hpp>

#include "Common/TreeFormatter.h"
#include "IR/Pass.h"

namespace scatha::ir {

/// Represents a local pass in the pipeline tree
class PipelineLocalNode {
public:
    explicit PipelineLocalNode(LocalPass pass): pass(std::move(pass)) {}

    bool execute(ir::Context& ctx, ir::Function& F) const {
        return pass(ctx, F);
    }

    void print(std::ostream&) const;

    void printTree(std::ostream&, TreeFormatter&) const;

private:
    LocalPass pass;
};

/// Represents a global pass in the pipeline tree
class PipelineGlobalNode {
public:
    PipelineGlobalNode(
        GlobalPass pass,
        utl::small_vector<std::unique_ptr<PipelineLocalNode>> children = {}):
        pass(std::move(pass)), children(std::move(children)) {}

    PipelineGlobalNode(GlobalPass pass,
                       std::unique_ptr<PipelineLocalNode> onlyChild):
        pass(std::move(pass)) {
        children.push_back(std::move(onlyChild));
    }

    bool execute(ir::Context& ctx, ir::Module& mod) const;

    void print(std::ostream&) const;

    void printTree(std::ostream&, TreeFormatter&) const;

private:
    GlobalPass pass;
    utl::small_vector<std::unique_ptr<PipelineLocalNode>> children;
};

/// Represents the root of the pipeline tree
class PipelineRoot {
public:
    PipelineRoot() = default;

    PipelineRoot(
        utl::small_vector<std::unique_ptr<PipelineGlobalNode>> children):
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
    utl::small_vector<std::unique_ptr<PipelineGlobalNode>> children;
};

} // namespace scatha::ir

#endif // SCATHA_IR_PIPELINENODES_H_
