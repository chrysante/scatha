#include "IR/CFG.h"

#include "IR/Context.h"

using namespace scatha;
using namespace ir;

void Value::addUserWeak(User* user) {
    auto const [itr, success] = _users.insert({ user, 0 });
    ++itr->second;
}

void Value::removeUserWeak(User* user) {
    auto itr = _users.find(user);
    SC_ASSERT(itr != _users.end(), "`user` is not a user of this value.");
    if (--itr->second == 0) {
        _users.erase(itr);
    }
}

void Value::privateDestroy() {
    visit(*this, [](auto& derived) { std::destroy_at(&derived); });
}

void Value::privateDelete() {
    visit(*this, [](auto& derived) { delete &derived; });
}

User::User(NodeType nodeType,
           Type const* type,
           std::string name,
           utl::small_vector<Value*> operands):
    Value(nodeType, type, std::move(name)) {
    setOperands(std::move(operands));
}

void User::setOperand(size_t index, Value* operand) {
    SC_ASSERT(index < _operands.size(), "");
    if (auto* op = _operands[index]) {
        op->removeUserWeak(this);
    }
    operand->addUserWeak(this);
    _operands[index] = operand;
}

void User::setOperands(utl::small_vector<Value*> operands) {
    clearOperands();
    _operands = std::move(operands);
    for (auto* op: _operands) {
        if (op) {
            op->addUserWeak(this);
        }
    }
}

void User::updateOperand(Value const* oldOperand, Value* newOperand) {
    auto itr = ranges::find(_operands, oldOperand);
    SC_ASSERT(itr != ranges::end(_operands), "Not found");
    auto const index = ranges::end(_operands) - itr;
    setOperand(utl::narrow_cast<size_t>(index), newOperand);
}

void User::removeOperand(size_t index) {
    auto const itr = _operands.begin() + index;
    (*itr)->removeUserWeak(this);
    _operands.erase(itr);
}

void User::clearOperands() {
    for (auto& op: _operands) {
        if (op) {
            op->removeUserWeak(this);
        }
        op = nullptr;
    }
}

IntegralConstant::IntegralConstant(Context& context,
                                   APInt value,
                                   size_t bitWidth):
    Constant(NodeType::IntegralConstant, context.integralType(bitWidth)),
    _value(value) {}

FloatingPointConstant::FloatingPointConstant(Context& context,
                                             APFloat value,
                                             size_t bitWidth):
    Constant(NodeType::FloatingPointConstant, context.floatType(bitWidth)),
    _value(value) {}

Instruction::Instruction(NodeType nodeType,
                         Type const* type,
                         std::string name,
                         utl::small_vector<Value*> operands,
                         utl::small_vector<Type const*> typeOperands):
    User(nodeType, type, std::move(name), std::move(operands)),
    typeOps(std::move(typeOperands)) {}

Function::Function(FunctionType const* functionType,
                   Type const* returnType,
                   std::span<Type const* const> parameterTypes,
                   std::string name):
    Constant(NodeType::Function, functionType, std::move(name)),
    _returnType(returnType) {
    for (int index = 0; auto* type: parameterTypes) {
        params.push_back(new Parameter(type, std::to_string(index++), this));
    }
}

BasicBlock::BasicBlock(Context& context, std::string name):
    Value(NodeType::BasicBlock, context.voidType(), std::move(name)) {}

/// Outlined because Function is incomplete at definition of BasicBlock.
bool BasicBlock::isEntry() const {
    return parent()->begin().to_address() == this;
}

bool BasicBlock::contains(Instruction const& inst) const {
    auto const itr = std::find_if(begin(), end(), [&](Instruction const& i) {
        return &i == &inst;
    });
    return itr != end();
}

TerminatorInst const* BasicBlock::terminator() const {
    if (empty()) {
        return nullptr;
    }
    return dyncast<TerminatorInst const*>(&back());
}

Alloca::Alloca(Context& context, Type const* allocatedType, std::string name):
    Instruction(NodeType::Alloca,
                context.pointerType(),
                std::move(name),
                {},
                { allocatedType }) {}

Store::Store(Context& context, Value* address, Value* value):
    BinaryInstruction(NodeType::Store, address, value, context.voidType()) {
    SC_ASSERT(isa<PointerType>(address->type()),
              "`address` must be of type `ptr`");
}

CompareInst::CompareInst(Context& context,
                         Value* lhs,
                         Value* rhs,
                         CompareOperation op,
                         std::string name):
    BinaryInstruction(NodeType::CompareInst,
                      lhs,
                      rhs,
                      context.integralType(1),
                      std::move(name)),
    _op(op) {
    SC_ASSERT(lhs->type() == rhs->type(), "Type mismatch");
    SC_ASSERT(isa<ArithmeticType>(lhs->type()),
              "Compared type must be arithmetic.");
}

UnaryArithmeticInst::UnaryArithmeticInst(Context& context,
                                         Value* operand,
                                         UnaryArithmeticOperation op,
                                         std::string name):
    UnaryInstruction(NodeType::UnaryArithmeticInst,
                     operand,
                     op == UnaryArithmeticOperation::LogicalNot ?
                         context.integralType(1) :
                         operand->type(),
                     std::move(name)),
    _op(op) {
    switch (op) {
    case UnaryArithmeticOperation::Negation:
        SC_ASSERT(isa<ArithmeticType>(operand->type()),
                  "Operand type must be arithmetic");
        break;
    case UnaryArithmeticOperation::BitwiseNot:
        SC_ASSERT(isa<IntegralType>(operand->type()),
                  "Operand type must be integral");
        break;
    case UnaryArithmeticOperation::LogicalNot:
        SC_ASSERT(isa<IntegralType>(operand->type()) &&
                      cast<IntegralType const*>(operand->type())->bitWidth() ==
                          1,
                  "Operand type must be i1");
        break;
    default: SC_UNREACHABLE();
    }
}

ArithmeticInst::ArithmeticInst(Value* lhs,
                               Value* rhs,
                               ArithmeticOperation op,
                               std::string name):
    BinaryInstruction(NodeType::ArithmeticInst,
                      lhs,
                      rhs,
                      lhs->type(),
                      std::move(name)),
    _op(op) {
    SC_ASSERT(lhs->type() == rhs->type(), "Type mismatch");
    SC_ASSERT(isa<ArithmeticType>(lhs->type()),
              "Operands types must be arithmetic");
}

TerminatorInst::TerminatorInst(NodeType nodeType,
                               Context& context,
                               std::initializer_list<Value*> operands,
                               std::initializer_list<BasicBlock*> targets):
    Instruction(nodeType, context.voidType(), {}),
    nonTargetArguments(utl::narrow_cast<uint16_t>(operands.size())) {
    utl::small_vector<Value*> ops;
    ops.reserve(operands.size() + targets.size());
    ranges::copy(operands, std::back_inserter(ops));
    ranges::copy(targets, std::back_inserter(ops));
    setOperands(std::move(ops));
}

FunctionCall::FunctionCall(Function* function,
                           std::span<Value* const> arguments,
                           std::string name):
    Instruction(NodeType::FunctionCall,
                function->returnType(),
                std::move(name)) {
    utl::small_vector<Value*> args;
    args.reserve(1 + arguments.size());
    args.push_back(function);
    ranges::copy(arguments, std::back_inserter(args));
    setOperands(std::move(args));
}

ExtFunctionCall::ExtFunctionCall(size_t slot,
                                 size_t index,
                                 std::string functionName,
                                 std::span<Value* const> arguments,
                                 ir::Type const* returnType,
                                 std::string name):
    Instruction(NodeType::ExtFunctionCall,
                returnType,
                std::move(name),
                arguments),
    _slot(utl::narrow_cast<u32>(slot)),
    _index(utl::narrow_cast<u32>(index)),
    _functionName(std::move(functionName)) {}

template <typename E>
static auto extract(std::span<PhiMapping const> args, E extractor) {
    using R = std::invoke_result_t<E, PhiMapping>;
    utl::small_vector<R> result;
    result.reserve(args.size());
    std::transform(args.begin(),
                   args.end(),
                   std::back_inserter(result),
                   extractor);
    return result;
}

void Phi::setArguments(std::span<PhiMapping const> args) {
    SC_ASSERT(!args.empty(), "Phi must have at least one argument");
    setType(args[0].value->type());
    setOperands(extract(args, [](PhiMapping p) { return p.value; }));
    _preds = extract(args, [](PhiMapping p) { return p.pred; });
    for (auto& [pred, value]: args) {
        SC_ASSERT(value->type() == type(), "Type mismatch");
    }
}

void Phi::setArgument(BasicBlock const* pred, Value* value) {
    setArgument(indexOf(pred), value);
}

void Phi::setArgument(size_t index, Value* value) {
    setOperand(index, value);
}

void Phi::setPredecessor(size_t index, BasicBlock* pred) {
    _preds[index] = pred;
}

GetElementPointer::GetElementPointer(Context& context,
                                     Type const* accessedType,
                                     Value* basePointer,
                                     Value* arrayIndex,
                                     std::span<size_t const> memberIndices,
                                     std::string name):
    Instruction(NodeType::GetElementPointer,
                context.pointerType(),
                std::move(name),
                { basePointer, arrayIndex },
                { accessedType }),
    _memberIndices(memberIndices | ranges::to<utl::small_vector<uint16_t>>) {
    SC_ASSERT(isa<PointerType>(basePointer->type()),
              "`basePointer` must be a pointer");
    SC_ASSERT(isa<IntegralType>(arrayIndex->type()),
              "Indices must be integral");
}

Type const* internal::AccessValueBase::computeAccessedType(
    Type const* operandType, std::span<size_t const> indices) {
    Type const* result = operandType;
    for (auto index: indices) {
        result = cast<StructureType const*>(result)->memberAt(index);
    }
    return result;
}
