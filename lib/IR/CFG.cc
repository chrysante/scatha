#include "IR/CFG.h"

#include "IR/Context.h"

using namespace scatha;
using namespace ir;

IntegralConstant::IntegralConstant(Context& context, APInt value, size_t bitWidth):
    Constant(NodeType::IntegralConstant, context.integralType(bitWidth)), _value(value) {}

FloatingPointConstant::FloatingPointConstant(Context& context, APFloat value, size_t bitWidth):
    Constant(NodeType::FloatingPointConstant, context.floatType(bitWidth)), _value(value) {}

Function::Function(FunctionType const* functionType,
                   Type const* returnType,
                   std::span<Type const* const> parameterTypes,
                   std::string name):
    Constant(NodeType::Function, functionType, std::move(name)), _returnType(returnType) {
    for (int index = 0; auto* type: parameterTypes) {
        params.push_back(new Parameter(type, std::to_string(index++), this));
    }
}

BasicBlock::BasicBlock(Context& context, std::string name):
    Value(NodeType::BasicBlock, context.voidType(), std::move(name)) {}

bool BasicBlock::isEntry() const {
    return parent()->basicBlocks().begin() == List<BasicBlock>::const_iterator(this);
}

Alloca::Alloca(Context& context, Type const* allocatedType, std::string name):
    Instruction(NodeType::Alloca, context.pointerType(), std::move(name)), _allocatedType(allocatedType) {}

Store::Store(Context& context, Value* address, Value* value):
    BinaryInstruction(NodeType::Store, address, value, context.voidType())
{
    SC_ASSERT(address->type()->isPointer(), "Address argument to Store must be a pointer");
}

CompareInst::CompareInst(Context& context, Value* lhs, Value* rhs, CompareOperation op, std::string name):
    BinaryInstruction(NodeType::CompareInst, lhs, rhs, context.integralType(1), std::move(name)), _op(op)
{
    SC_ASSERT(lhs->type() == rhs->type(), "Type mismatch");
}

UnaryArithmeticInst::UnaryArithmeticInst(Context& context, Value* operand, UnaryArithmeticOperation op, std::string name):
    UnaryInstruction(NodeType::UnaryArithmeticInst,
                     operand,
                     op == UnaryArithmeticOperation::LogicalNot ? context.integralType(1) : operand->type(),
                     std::move(name)),
    _op(op) {}

TerminatorInst::TerminatorInst(NodeType nodeType, Context& context): Instruction(nodeType, context.voidType()) {}

FunctionCall::FunctionCall(Function* function, std::span<Value* const> arguments, std::string name):
    Instruction(NodeType::FunctionCall, function->returnType(), std::move(name)),
    _function(function),
    _args(arguments) {}
