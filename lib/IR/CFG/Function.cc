#include "IR/CFG/Function.h"

#include <optional>

#include <range/v3/view.hpp>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Dominance.h"
#include "IR/Loop.h"
#include "IR/Type.h"

using namespace scatha;
using namespace ir;
using namespace ranges::views;

Parameter::Parameter(Type const* type, size_t index, Callable* parent):
    Parameter(type, index, std::to_string(index), parent) {}

Parameter::Parameter(Type const* type,
                     size_t index,
                     std::string name,
                     Callable* parent):
    Value(NodeType::Parameter, type, std::move(name)),
    ParentNodeBase(parent),
    _index(index) {}

List<Parameter> ir::makeParameters(std::span<Type const* const> types) {
    List<Parameter> result;
    for (auto [index, type]: types | ranges::views::enumerate) {
        result.push_back(new Parameter(type, index++, nullptr));
    }
    return result;
}

Callable::Callable(NodeType nodeType,
                   Context& ctx,
                   Type const* returnType,
                   List<Parameter> parameters,
                   std::string name,
                   FunctionAttribute attr,
                   Visibility vis):
    Global(nodeType, ctx.ptrType(), std::move(name)),
    params(std::move(parameters)),
    _returnType(returnType),
    attrs(attr),
    vis(vis) {
    for (auto [index, param]: params | ranges::views::enumerate) {
        param.setIndex(index);
        param.set_parent(this);
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

Function::Function(Context& ctx,
                   Type const* returnType,
                   List<Parameter> parameters,
                   std::string name,
                   FunctionAttribute attr,
                   Visibility vis):
    Callable(NodeType::Function,
             ctx,
             returnType,
             std::move(parameters),
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

void Function::writeValueToImpl(
    void* dest,
    utl::function_view<void(Constant const*, void*)> callback) const {
    /// For debug reasons
    uint64_t const placeholder = 0xDeadBeefAbbaACDC;
    std::memcpy(dest, &placeholder, 8);
    SC_ASSERT(callback,
              "We need a callback that registers this label placeholder");
    callback(this, dest);
}

static FFIType toFFIType(Type const* type) {
    // clang-format off
    return SC_MATCH_R (FFIType, *type) {
        [](VoidType const&) { return FFIType::Void; },
        [](IntegralType const& type) {
            switch (type.bitwidth()) {
            case 1:
                return FFIType::Int8;
            case 8:
                return FFIType::Int8;
            case 16:
                return FFIType::Int16;
            case 32:
                return FFIType::Int32;
            case 64:
                return FFIType::Int64;
                
            default:
                SC_UNREACHABLE("Invalid FFI type");
            }
        },
        [](FloatType const& type) {
            switch (type.bitwidth()) {
            case 32:
                return FFIType::Float;
            case 64:
                return FFIType::Double;
                
            default:
                SC_UNREACHABLE("Invalid FFI type");
            }
        },
        [](PointerType const&) { return FFIType::Pointer; },
        [](Type const&) { SC_UNREACHABLE("Invalid FFI type"); }
    }; // clang-format on
}

static ForeignFunctionInterface makeFFI(std::string name,
                                        Type const* retType,
                                        std::span<Type const* const> argTypes) {
    return ForeignFunctionInterface(std::move(name),
                                    argTypes | transform(toFFIType) |
                                        ToSmallVector<>,
                                    toFFIType(retType));
}

ForeignFunction::ForeignFunction(Context& ctx,
                                 Type const* returnType,
                                 List<Parameter> parameters,
                                 std::string name,
                                 FunctionAttribute attr):
    Callable(NodeType::ForeignFunction,
             ctx,
             returnType,
             std::move(parameters),
             name,
             attr,
             Visibility::Internal),
    ffi(makeFFI(name,
                returnType,
                this->parameters() | transform(&Parameter::type) |
                    ToSmallVector<>)) {}

void ForeignFunction::writeValueToImpl(
    void*, utl::function_view<void(Constant const*, void*)>) const {
    SC_UNREACHABLE("We cannot write the address because foreign functions are "
                   "not addressable");
}
