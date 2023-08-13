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

std::ostream& opt::operator<<(std::ostream& ostream, Pipeline const& pipeline) {
    pipeline.root->print(ostream);
    return ostream;
}

void opt::print(Pipeline const& pipeline) { print(pipeline, std::cout); }

void opt::print(Pipeline const& pipeline, std::ostream& ostream) {
    pipeline.root->print(ostream);
    ostream << "\n";
}

void opt::printTree(Pipeline const& pipeline) {
    printTree(pipeline, std::cout);
}

void opt::printTree(Pipeline const& pipeline, std::ostream& ostream) {
    TreeFormatter formatter;
    pipeline.root->printTree(ostream, formatter);
}
