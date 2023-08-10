#ifndef SCATHA_OPT_PIPELINE_H_
#define SCATHA_OPT_PIPELINE_H_

#include <iosfwd>
#include <memory>

#include "Common/Base.h"
#include "IR/Fwd.h"

namespace scatha::opt {

class Pipeline;
class PipelineRoot;

SCATHA_API void print(Pipeline const& pipeline);

SCATHA_API void print(Pipeline const& pipeline, std::ostream& ostream);

class SCATHA_API Pipeline {
public:
    Pipeline();

    explicit Pipeline(std::unique_ptr<PipelineRoot> root);

    Pipeline(Pipeline&&) noexcept;

    Pipeline& operator=(Pipeline&&) noexcept;

    ~Pipeline();

    /// Execute this pipeline on the module \p mod
    bool execute(ir::Context& ctx, ir::Module& mod) const;

    /// Calls `execute()`
    bool operator()(ir::Context& ctx, ir::Module& mod) const {
        return execute(ctx, mod);
    }

private:
    friend void print(Pipeline const& pipeline, std::ostream& ostream);
    std::unique_ptr<PipelineRoot> root;
};

} // namespace scatha::opt

#endif // SCATHA_OPT_PIPELINE_H_
