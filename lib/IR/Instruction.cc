#include "IR/Instruction.h"

#include "IR/Function.h"

using namespace scatha;
using namespace ir;

FunctionCall::FunctionCall(Function* function, std::span<Value* const> arguments, std::string name):
    Instruction(NodeType::FunctionCall, function->returnType(), std::move(name)),
    _function(function),
    _args(arguments) {}
