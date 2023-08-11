#include "Opt/Pipeline.h"

#include <iostream>

#include "Opt/Pipeline/PipelineNodes.h"

using namespace scatha;
using namespace opt;

Pipeline::Pipeline(): root(std::make_unique<PipelineRoot>()) {}

Pipeline::Pipeline(std::unique_ptr<PipelineRoot> root): root(std::move(root)) {}

Pipeline::Pipeline(Pipeline&&) noexcept = default;

Pipeline& Pipeline::operator=(Pipeline&&) noexcept = default;

Pipeline::~Pipeline() = default;

bool Pipeline::execute(ir::Context& ctx, ir::Module& mod) const {
    SC_ASSERT(root, "Invalid pipeline");
    return root->execute(ctx, mod);
}

bool Pipeline::empty() const { return root->empty(); }

SCATHA_API void opt::print(Pipeline const& pipeline) {
    print(pipeline, std::cout);
}

SCATHA_API void opt::print(Pipeline const& pipeline, std::ostream& ostream) {
    TreeFormatter formatter;
    pipeline.root->print(ostream, formatter);
}
