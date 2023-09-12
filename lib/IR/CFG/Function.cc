#include "IR/CFG/Function.h"

#include <optional>

#include "IR/Dominance.h"
#include "IR/Loop.h"
#include "IR/Type.h"

using namespace scatha;
using namespace ir;

static utl::small_vector<Parameter*> toParameters(
    std::span<Type const* const> types, Callable* func) {
    utl::small_vector<Parameter*> result;
    for (auto [index, type]: types | ranges::views::enumerate) {
        auto* p = new Parameter(type, index++, func);
        result.push_back(p);
    }
    return result;
}

Callable::Callable(NodeType nodeType,
                   FunctionType const* functionType,
                   Type const* returnType,
                   std::span<Type const* const> parameterTypes,
                   std::string name,
                   FunctionAttribute attr,
                   Visibility vis):
    Callable(nodeType,
             functionType,
             returnType,
             toParameters(parameterTypes, this),
             std::move(name),
             attr,
             vis) {}

Callable::Callable(NodeType nodeType,
                   FunctionType const* functionType,
                   Type const* returnType,
                   std::span<Parameter* const> parameters,
                   std::string name,
                   FunctionAttribute attr,
                   Visibility vis):
    Constant(nodeType, functionType, std::move(name)),
    _returnType(returnType),
    attrs(attr),
    vis(vis) {
    for (auto [index, param]: parameters | ranges::views::enumerate) {
        param->setIndex(index);
        param->set_parent(this);
        params.push_back(param);
    }
}

struct Function::AnalysisData {
    std::optional<DominanceInfo> domInfo;
    std::optional<DominanceInfo> postDomInfo;
    std::optional<LoopNestingForest> LNF;
};

static void uniqueParams(auto&& params, auto&& nameFac) {
    for (auto& param: params) {
        bool const nameUnique = nameFac.tryRegister(param.name());
        SC_ASSERT(nameUnique, "How are the parameter names not unique?");
    }
}

Function::Function(FunctionType const* functionType,
                   Type const* returnType,
                   std::span<Type const* const> parameterTypes,
                   std::string name,
                   FunctionAttribute attr,
                   Visibility vis):
    Callable(NodeType::Function,
             functionType,
             returnType,
             parameterTypes,
             std::move(name),
             attr,
             vis),
    nameFac(),
    analysisData(std::make_unique<AnalysisData>()) {
    uniqueParams(parameters(), nameFac);
}

Function::Function(FunctionType const* functionType,
                   Type const* returnType,
                   std::span<Parameter* const> parameters,
                   std::string name,
                   FunctionAttribute attr,
                   Visibility vis):
    Callable(NodeType::Function,
             functionType,
             returnType,
             parameters,
             std::move(name),
             attr,
             vis),
    nameFac(),
    analysisData(std::make_unique<AnalysisData>()) {
    uniqueParams(this->parameters(), nameFac);
}

Function::~Function() = default;

DomTree const& Function::getOrComputeDomTree() const {
    return getOrComputeDomInfo().domTree();
}

DominanceInfo const& Function::getOrComputeDomInfo() const {
    if (!analysisData->domInfo) {
        analysisData->domInfo =
            DominanceInfo::compute(const_cast<Function&>(*this));
    }
    return *analysisData->domInfo;
}

DominanceInfo const& Function::getOrComputePostDomInfo() const {
    if (!analysisData->postDomInfo) {
        analysisData->postDomInfo =
            DominanceInfo::computePost(const_cast<Function&>(*this));
    }
    return *analysisData->postDomInfo;
}

LoopNestingForest const& Function::getOrComputeLNF() const {
    if (!analysisData->LNF) {
        analysisData->LNF =
            LoopNestingForest::compute(const_cast<Function&>(*this),
                                       getOrComputeDomTree());
    }
    return *analysisData->LNF;
}

void Function::invalidateCFGInfo() {
    analysisData->domInfo = std::nullopt;
    analysisData->postDomInfo = std::nullopt;
    analysisData->LNF = std::nullopt;
}

void Function::insertCallback(BasicBlock& bb) {
    bb.set_parent(this);
    bb.uniqueExistingName(*this);
    for (auto& inst: bb) {
        bb.insertCallback(inst);
    }
}

void Function::eraseCallback(BasicBlock const& bb) {
    nameFac.erase(bb.name());
    for (auto& inst: bb) {
        const_cast<BasicBlock&>(bb).eraseCallback(inst);
    }
}

ForeignFunction::ForeignFunction(FunctionType const* functionType,
                         Type const* returnType,
                         std::span<Type const* const> parameterTypes,
                         std::string name,
                         uint32_t slot,
                         uint32_t index,
                         FunctionAttribute attr):
    Callable(NodeType::ForeignFunction,
             functionType,
             returnType,
             parameterTypes,
             std::move(name),
             attr,
             Visibility::Static),
    _slot(slot),
    _index(index) {}

ForeignFunction::ForeignFunction(FunctionType const* functionType,
                         Type const* returnType,
                         std::span<Parameter* const> parameters,
                         std::string name,
                         uint32_t slot,
                         uint32_t index,
                         FunctionAttribute attr):
    Callable(NodeType::ForeignFunction,
             functionType,
             returnType,
             parameters,
             std::move(name),
             attr,
             Visibility::Static),
    _slot(slot),
    _index(index) {}
