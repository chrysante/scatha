#include "IR/CFG/Function.h"

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
                   FunctionAttribute attr):
    Callable(nodeType,
             functionType,
             returnType,
             toParameters(parameterTypes, this),
             std::move(name),
             attr) {}

Callable::Callable(NodeType nodeType,
                   FunctionType const* functionType,
                   Type const* returnType,
                   std::span<Parameter* const> parameters,
                   std::string name,
                   FunctionAttribute attr):
    Constant(nodeType, functionType, std::move(name)),
    _returnType(returnType),
    attrs(attr) {
    for (auto [index, param]: parameters | ranges::views::enumerate) {
        param->setIndex(index);
        param->set_parent(this);
        params.push_back(param);
    }
}

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
                   FunctionAttribute attr):
    Callable(NodeType::Function,
             functionType,
             returnType,
             parameterTypes,
             std::move(name),
             attr) {
    uniqueParams(parameters(), nameFac);
}

Function::Function(FunctionType const* functionType,
                   Type const* returnType,
                   std::span<Parameter* const> parameters,
                   std::string name,
                   FunctionAttribute attr):
    Callable(NodeType::Function,
             functionType,
             returnType,
             parameters,
             std::move(name),
             attr) {
    uniqueParams(this->parameters(), nameFac);
}

void Function::clear() {
    for (auto& inst: instructions()) {
        inst.clearOperands();
    }
    values.clear();
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

ExtFunction::ExtFunction(FunctionType const* functionType,
                         Type const* returnType,
                         std::span<Type const* const> parameterTypes,
                         std::string name,
                         uint32_t slot,
                         uint32_t index,
                         FunctionAttribute attr):
    Callable(NodeType::ExtFunction,
             functionType,
             returnType,
             parameterTypes,
             std::move(name),
             attr),
    _slot(slot),
    _index(index) {}
