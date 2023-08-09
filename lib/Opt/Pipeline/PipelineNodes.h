#ifndef SCATHA_OPT_PIPELINENODES_H_
#define SCATHA_OPT_PIPELINENODES_H_

#include <iosfwd>
#include <memory>

#include <utl/vector.hpp>

#include "Common/TreeFormatter.h"
#include "Opt/Pass.h"

namespace scatha::opt {

/// Represents a local pass in the pipeline tree
class PipelineLocalNode {
public:
    explicit PipelineLocalNode(LocalPass pass): pass(std::move(pass)) {}

    bool execute(ir::Context& ctx, ir::Function& F) const {
        return pass(ctx, F);
    }

    void print(std::ostream&, TreeFormatter&) const;

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

    bool execute(ir::Context& ctx, ir::Module& mod) const {
        return pass(ctx,
                    mod,
                    LocalPass([this](ir::Context& ctx, ir::Function& F) {
                        bool result = false;
                        for (auto& child: children) {
                            result |= child->execute(ctx, F);
                        }
                        return result;
                    }));
    }

    void print(std::ostream&, TreeFormatter&) const;

private:
    GlobalPass pass;
    utl::small_vector<std::unique_ptr<PipelineLocalNode>> children;
};

/// Represents the root of the pipeline tree
class PipelineRoot {
public:
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

    void print(std::ostream&, TreeFormatter&) const;

private:
    utl::small_vector<std::unique_ptr<PipelineGlobalNode>> children;
};

} // namespace scatha::opt

#endif // SCATHA_OPT_PIPELINENODES_H_
