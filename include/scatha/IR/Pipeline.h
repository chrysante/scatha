#ifndef SCATHA_IR_PIPELINE_H_
#define SCATHA_IR_PIPELINE_H_

#include <iosfwd>
#include <memory>
#include <string>

#include <scatha/Common/Base.h>
#include <scatha/IR/Fwd.h>

namespace scatha::ir {

class Pipeline;
class PipelineRoot;

/// Print a descriptive string of the pipeline to \p ostream
/// Equivalent to `ostream << toString(pipeline)`
SCATHA_API std::ostream& operator<<(std::ostream& ostream,
                                    Pipeline const& pipeline);

/// Generate a descriptive string of the pipeline of the form
/// `global(locals,...),...`
SCATHA_API std::string toString(Pipeline const& pipeline);

/// Print \p pipeline as a flat list of passes
SCATHA_API void print(Pipeline const& pipeline);

/// \overload
SCATHA_API void print(Pipeline const& pipeline, std::ostream& ostream);

/// Print \p pipeline as a tree
SCATHA_API void printTree(Pipeline const& pipeline);

/// \overload
SCATHA_API void printTree(Pipeline const& pipeline, std::ostream& ostream);

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

    /// \returns `true` if this pipeline is not empty
    bool empty() const;

    /// \returns `!empty()`
    explicit operator bool() const { return !empty(); }

    /// Construct a pipeline from a pipeline root node.
    /// \Note This API is private
    explicit Pipeline(std::unique_ptr<PipelineRoot> root);

private:
    friend std::ostream& operator<<(std::ostream&, Pipeline const&);
    friend void print(Pipeline const&, std::ostream&);
    friend void printTree(Pipeline const&, std::ostream&);

    std::unique_ptr<PipelineRoot> root;
};

} // namespace scatha::ir

#endif // SCATHA_IR_PIPELINE_H_
