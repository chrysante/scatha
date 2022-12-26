#include "IR/Function.h"

#include "IR/BasicBlock.h"
#include "IR/Parameter.h"

using namespace scatha;
using namespace ir;

Function::Function(FunctionType const* functionType,
                   Type const* returnType,
                   std::span<Type const* const> parameterTypes,
                   std::string name):
    Constant(NodeType::Function, functionType, std::move(name)), _returnType(returnType) {
    for (int index = 0; auto* type: parameterTypes) {
        params.push_back(new Parameter(type, std::to_string(index++), this));
    }
}

Function::~Function() = default;

void Function::addBasicBlock(BasicBlock* bb) {
    bbs.push_back(bb);
}
