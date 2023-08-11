#ifndef SCATHA_OPT_PIPELINE_H_
#define SCATHA_OPT_PIPELINE_H_

#include <iosfwd>
#include <memory>

#include <scatha/Common/Base.h>
#include <scatha/IR/Fwd.h>

namespace scatha::opt {

class Pipeline;
class PipelineRoot;

/// Print \p pipeline to `std::cout`
SCATHA_API void print(Pipeline const& pipeline);

/// Print \p pipeline to \p ostream
SCATHA_API void print(Pipeline const& pipeline, std::ostream& ostream);

/// Represents an optimization pipeline, i.e. a sequence of global and nested
/// local passes.
class SCATHA_API Pipeline {
public:
    /// Construct an empty pipeline. An empty pipeline is a no-op and also
    /// returns false when executed.
    Pipeline();

    Pipeline(Pipeline&&) noexcept;

    Pipeline& operator=(Pipeline&&) noexcept;

    ~Pipeline();

    /// Execute this pipeline on the module \p mod
    bool execute(ir::Context& ctx, ir::Module& mod) const;

    /// Calls `execute()`
    bool operator()(ir::Context& ctx, ir::Module& mod) const {
        return execute(ctx, mod);
    }

    /// Construct a pipeline from a pipeline root node.
    /// \Note This API is private
    explicit Pipeline(std::unique_ptr<PipelineRoot> root);

private:
    friend void print(Pipeline const& pipeline, std::ostream& ostream);

    std::unique_ptr<PipelineRoot> root;
};

} // namespace scatha::opt

#endif // SCATHA_OPT_PIPELINE_H_
