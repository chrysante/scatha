#include "IR/Function.h"

#include "IR/BasicBlock.h"
#include "IR/Parameter.h"

using namespace scatha;
using namespace ir;

Function::Function(FunctionType* functionType, std::span<Type const*> parameterTypes):
    Constant(NodeType::Function, functionType)
{
    for (auto* type: parameterTypes) {
        params.push_back(new Parameter(type, this));
    }
}

Function::~Function() = default;

void Function::addBasicBlock(BasicBlock* bb) { bbs.push_back(bb); }
