#include "IR/Pipeline.h"

#include <iostream>
#include <sstream>

#include "IR/ForEach.h"
#include "IR/PipelineNodes.h"

using namespace scatha;
using namespace ir;

Pipeline::Pipeline(): root(std::make_unique<PipelineRoot>()) {}

Pipeline::Pipeline(std::unique_ptr<PipelineRoot> root): root(std::move(root)) {}

Pipeline::Pipeline(Pipeline&&) noexcept = default;

Pipeline& Pipeline::operator=(Pipeline&&) noexcept = default;

Pipeline::~Pipeline() = default;

static auto passToPipelineRoot(ModulePass global, FunctionPass local) {
    auto localNode = std::make_unique<PipelineFunctionNode>(std::move(local));
    auto globalNode =
        std::make_unique<PipelineModuleNode>(std::move(global),
                                             std::move(localNode));
    return std::make_unique<PipelineRoot>(std::move(globalNode));
}

static auto passToPipelineRoot(FunctionPass pass) {
    return passToPipelineRoot(ir::forEach, std::move(pass));
}

Pipeline::Pipeline(FunctionPass pass) noexcept:
    Pipeline(passToPipelineRoot(std::move(pass))) {}

Pipeline::Pipeline(ModulePass global, FunctionPass local) noexcept:
    Pipeline(passToPipelineRoot(std::move(global), std::move(local))) {}

bool Pipeline::execute(ir::Context& ctx, ir::Module& mod) const {
    SC_ASSERT(root, "Invalid pipeline");
    return root->execute(ctx, mod);
}

bool Pipeline::empty() const { return root->empty(); }

std::ostream& ir::operator<<(std::ostream& ostream, Pipeline const& pipeline) {
    pipeline.root->print(ostream);
    return ostream;
}

std::string ir::toString(Pipeline const& pipeline) {
    std::stringstream sstr;
    sstr << pipeline;
    return std::move(sstr).str();
}

void ir::print(Pipeline const& pipeline) { print(pipeline, std::cout); }

void ir::print(Pipeline const& pipeline, std::ostream& ostream) {
    pipeline.root->print(ostream);
    ostream << "\n";
}

void ir::printTree(Pipeline const& pipeline) { printTree(pipeline, std::cout); }

void ir::printTree(Pipeline const& pipeline, std::ostream& ostream) {
    TreeFormatter formatter;
    pipeline.root->printTree(ostream, formatter);
}
