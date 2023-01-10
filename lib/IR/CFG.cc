#include "IR/CFG.h"

#include "IR/Context.h"

using namespace scatha;
using namespace ir;

void Value::addUser(User* user) {
    auto const [itr, success] = _users.insert({ user, 0 });
    ++itr->second;
}

void Value::removeUser(User* user) {
    auto itr = _users.find(user);
    SC_ASSERT(itr != _users.end(), "\p user is not a user of this value.");
    if (--itr->second == 0) {
        _users.erase(itr);
    }
}

User::User(NodeType nodeType, Type const* type, std::string name, std::span<Value* const> operands):
    Value(nodeType, type, std::move(name)) {
    for (Value* op: operands) {
        if (!op) { continue; }
        _operands.push_back(op);
        op->addUser(this);
    }
}

void User::setOperand(size_t index, Value* operand) {
    SC_ASSERT(index < _operands.size(), "");
    _operands[index]->removeUser(this);
    operand->addUser(this);
    _operands[index] = operand;
}

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

bool BasicBlock::contains(Instruction const& inst) const {
    return std::find_if(instructions.begin(), instructions.end(), [&](Instruction const& i) {
        return &i == &inst;
    }) != instructions.end();
}

TerminatorInst const* BasicBlock::terminator() const {
    if (instructions.empty()) { return nullptr; }
    return dyncast<TerminatorInst const*>(&instructions.back());
}

static auto succImpl(auto* t) {
    return t ? t->targets() : std::span<BasicBlock* const>{};
}

std::span<BasicBlock* const> BasicBlock::successors() {
    return succImpl(terminator());
}

std::span<BasicBlock const* const> BasicBlock::successors() const {
    return succImpl(terminator());
}

Alloca::Alloca(Context& context, Type const* allocatedType, std::string name):
    Instruction(NodeType::Alloca, context.pointerType(allocatedType), std::move(name)), _allocatedType(allocatedType) {}

Store::Store(Context& context, Value* dest, Value* source):
    BinaryInstruction(NodeType::Store, dest, source, context.voidType()) {
    SC_ASSERT(cast<PointerType const*>(dest->type())->pointeeType() == source->type(),
              "dest must be a pointer to type of source");
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
    _op(op) {
    switch (op) {
    case UnaryArithmeticOperation::Promotion:
        SC_DEBUGBREAK(); // Why do we even generate these?
        break;
    case UnaryArithmeticOperation::Negation:
        SC_ASSERT(isa<ArithmeticType>(operand->type()), "Operand type must be arithmetic");
        break;
    case UnaryArithmeticOperation::BitwiseNot:
        SC_ASSERT(isa<IntegralType>(operand->type()), "Operand type must be integral");
        break;
    case UnaryArithmeticOperation::LogicalNot:
        SC_ASSERT(isa<IntegralType>(operand->type()) && cast<IntegralType const*>(operand->type())->bitWidth() == 1,
                  "Operand type must be i1");
        break;
    default: SC_UNREACHABLE();
    }
}

ArithmeticInst::ArithmeticInst(Value* lhs, Value* rhs, ArithmeticOperation op, std::string name):
    BinaryInstruction(NodeType::ArithmeticInst, lhs, rhs, lhs->type(), std::move(name)), _op(op) {
    SC_ASSERT(lhs->type() == rhs->type(), "Type mismatch");
    SC_ASSERT(isa<ArithmeticType>(lhs->type()), "Operands types must be arithmetic");
}

TerminatorInst::TerminatorInst(NodeType nodeType, Context& context, utl::small_vector<BasicBlock*> targets, std::initializer_list<Value*> operands):
    Instruction(nodeType, context.voidType(), {}, operands), _targets(std::move(targets)) {}

FunctionCall::FunctionCall(Function* function, std::span<Value* const> arguments, std::string name):
    Instruction(NodeType::FunctionCall, function->returnType(), std::move(name), arguments),
    _function(function) {}

ExtFunctionCall::ExtFunctionCall(
    size_t slot, size_t index, std::span<Value* const> arguments, ir::Type const* returnType, std::string name):
    Instruction(NodeType::ExtFunctionCall, returnType, std::move(name), arguments),
    _slot(utl::narrow_cast<u32>(slot)),
    _index(utl::narrow_cast<u32>(index)) {}

template <typename E>
static auto extract(std::span<PhiMapping const> args, E extractor) {
    using R = std::invoke_result_t<E, PhiMapping>;
    utl::small_vector<R> result;
    result.reserve(args.size());
    std::transform(args.begin(), args.end(), std::back_inserter(result), extractor);
    return result;
}

Phi::Phi(std::span<PhiMapping const> args, std::string name):
    Instruction(NodeType::Phi,
                (SC_ASSERT(!args.empty(), "Phi must have at least one argument"), args[0].value->type()),
                std::move(name),
                extract(args, [](PhiMapping p) { return p.value; })),
    _preds(extract(args, [](PhiMapping p) { return p.pred; })) {
    for (auto& [pred, value]: args) {
        SC_ASSERT(value->type() == type(), "Type mismatch");
    }
}

void Phi::clearArguments() {
    for (auto* op: operands()) {
        op->removeUser(this);
    }
    _operands.clear();
}

// WARNING: This will not work on arrays. We need to rework this for arrays.
GetElementPointer::GetElementPointer(
    Context& context, Type const* accessedType, Value* basePointer, size_t offsetIndex, std::string name):
    Instruction(NodeType::GetElementPointer,
                context.pointerType(cast<StructureType const*>(accessedType)->memberAt(offsetIndex)),
                std::move(name),
                { basePointer }),
    accType(accessedType),
    offsetIdx(offsetIndex) {}
