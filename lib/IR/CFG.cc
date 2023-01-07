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
    Instruction(NodeType::Alloca, context.pointerType(allocatedType), std::move(name)), _allocatedType(allocatedType) {}

Store::Store(Context& context, Value* dest, Value* source):
    BinaryInstruction(NodeType::Store, dest, source, context.voidType()) {
    SC_ASSERT(cast<PointerType const*>(dest->type())->pointeeType() == source->type(), "dest must be a pointer to type of source");
}

CompareInst::CompareInst(Context& context, Value* lhs, Value* rhs, CompareOperation op, std::string name):
    BinaryInstruction(NodeType::CompareInst, lhs, rhs, context.integralType(1), std::move(name)), _op(op) {
    SC_ASSERT(lhs->type() == rhs->type(), "Type mismatch");
    SC_ASSERT(isa<ArithmeticType>(lhs->type()), "Compared type must be arithmetic.");
}

UnaryArithmeticInst::UnaryArithmeticInst(Context& context,
                                         Value* operand,
                                         UnaryArithmeticOperation op,
                                         std::string name):
    UnaryInstruction(NodeType::UnaryArithmeticInst,
                     operand,
                     op == UnaryArithmeticOperation::LogicalNot ? context.integralType(1) : operand->type(),
                     std::move(name)),
    _op(op)
{
    SC_ASSERT(isa<IntegralType>(operand->type()) ||
              (op == UnaryArithmeticOperation::Negation && isa<FloatType>(operand->type())),
              "Operand type must be integral or float (for negation)");
}

ArithmeticInst::ArithmeticInst(Value* lhs, Value* rhs, ArithmeticOperation op, std::string name):
    BinaryInstruction(NodeType::ArithmeticInst, lhs, rhs, lhs->type(), std::move(name)), _op(op)
{
    SC_ASSERT(lhs->type() == rhs->type(), "Type mismatch");
    SC_ASSERT(isa<ArithmeticType>(lhs->type()), "Operands types must be arithmetic");
}

TerminatorInst::TerminatorInst(NodeType nodeType, Context& context): Instruction(nodeType, context.voidType()) {}

FunctionCall::FunctionCall(Function* function, std::span<Value* const> arguments, std::string name):
    Instruction(NodeType::FunctionCall, function->returnType(), std::move(name)),
    _function(function),
    _args(arguments) {}

ExtFunctionCall::ExtFunctionCall(
    size_t slot, size_t index, std::span<Value* const> arguments, ir::Type const* returnType, std::string name):
    Instruction(NodeType::ExtFunctionCall, returnType, std::move(name)),
    _slot(utl::narrow_cast<u32>(slot)),
    _index(utl::narrow_cast<u32>(index)),
    _args(arguments) {}

Phi::Phi(std::span<PhiMapping const> args, std::string name):
    Instruction(NodeType::Phi,
                (SC_ASSERT(!args.empty(), "Phi must have at least one argument"),
                 args[0].value->type()),
                std::move(name)),
    _arguments(args)
{
    for ([[maybe_unused]] auto& [pred, val]: args) {
        SC_ASSERT(val->type() == type(), "Type mismatch");
    }
}

GetElementPointer::GetElementPointer(
    Context& context, Type const* accessedType, Value* basePointer, size_t offsetIndex, std::string name):
    Instruction(NodeType::GetElementPointer,
                context.pointerType(cast<StructureType const*>(accessedType)->memberAt(offsetIndex)),
                std::move(name)),
    accType(accessedType),
    basePtr(basePointer),
    offsetIdx(offsetIndex) {}
