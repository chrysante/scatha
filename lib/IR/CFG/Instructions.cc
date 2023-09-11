#include "IR/CFG/Instructions.h"

#include <range/v3/algorithm.hpp>

#include "IR/CFG/Constants.h"
#include "IR/CFG/Function.h"
#include "IR/Context.h"
#include "IR/Type.h"

using namespace scatha;
using namespace ir;

Alloca::Alloca(Context& context, Type const* allocatedType, std::string name):
    Alloca(context,
           context.intConstant(1, 32),
           allocatedType,
           std::move(name)) {}

Alloca::Alloca(Context& context,
               Value* count,
               Type const* allocatedType,
               std::string name):
    Instruction(NodeType::Alloca,
                context.ptrType(),
                std::move(name),
                { count },
                { allocatedType }) {}

void Load::setAddress(Value* address) { UnaryInstruction::setOperand(address); }

Store::Store(Context& context, Value* address, Value* value):
    Instruction(NodeType::Store,
                context.voidType(),
                std::string{},
                { address, value }) {}

void Store::setAddress(Value* address) { setOperand(0, address); }

void Store::setValue(Value* value) { setOperand(1, value); }

ArithmeticType const* ConversionInst::type() const {
    auto* t = UnaryInstruction::type();
    if (!t) {
        return nullptr;
    }
    return cast<ArithmeticType const*>(t);
}

CompareInst::CompareInst(Context& context,
                         Value* lhs,
                         Value* rhs,
                         CompareMode mode,
                         CompareOperation op,
                         std::string name):
    BinaryInstruction(NodeType::CompareInst,
                      lhs,
                      rhs,
                      context.intType(1),
                      std::move(name)),
    _mode(mode),
    _op(op) {}

static Type const* computeUAType(Context& context,
                                 Value* operand,
                                 UnaryArithmeticOperation op) {
    if (!operand) {
        return nullptr;
    }
    if (op == UnaryArithmeticOperation::LogicalNot) {
        return context.intType(1);
    }
    return operand->type();
}

UnaryArithmeticInst::UnaryArithmeticInst(Context& context,
                                         Value* operand,
                                         UnaryArithmeticOperation op,
                                         std::string name):
    UnaryInstruction(NodeType::UnaryArithmeticInst,
                     operand,
                     computeUAType(context, operand, op),
                     std::move(name)),
    _op(op) {}

void UnaryArithmeticInst::setOperand(Context& context, Value* value) {
    User::setOperand(0, value);
    setType(computeUAType(context, value, operation()));
}

ArithmeticInst::ArithmeticInst(Value* lhs,
                               Value* rhs,
                               ArithmeticOperation op,
                               std::string name):
    BinaryInstruction(NodeType::ArithmeticInst,
                      lhs,
                      rhs,
                      lhs ? lhs->type() : nullptr,
                      std::move(name)),
    _op(op) {}

ArithmeticType const* ArithmeticInst::type() const {
    auto* t = Value::type();
    if (!t) {
        return nullptr;
    }
    return cast<ArithmeticType const*>(t);
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

void TerminatorInst::updateTarget(BasicBlock* oldTarget,
                                  BasicBlock* newTarget) {
    updateOperand(oldTarget, newTarget);
}

void TerminatorInst::setTarget(size_t index, BasicBlock* bb) {
    setOperand(nonTargetArguments + index, bb);
}

Return::Return(Context& context): Return(context, context.voidValue()) {}

Call::Call(Callable* function, std::string name):
    Call(function, std::span<Value* const>{}, std::move(name)) {}

Call::Call(Callable* function,
           std::span<Value* const> arguments,
           std::string name):
    Instruction(NodeType::Call,
                function ? function->returnType() : nullptr,
                std::move(name)) {
    utl::small_vector<Value*> args;
    args.reserve(1 + arguments.size());
    args.push_back(function);
    ranges::copy(arguments, std::back_inserter(args));
    setOperands(std::move(args));
}

void Call::setFunction(Callable* function) {
    setType(function ? function->returnType() : nullptr);
    setOperand(0, function);
}

Callable const* Call::function() const {
    return cast<Callable const*>(operands()[0]);
}

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
    Value* val = nullptr;
    if (!args.empty() && (val = args.front().value)) {
        setType(val->type());
    }
    setOperands(extract(args, [](PhiMapping p) { return p.value; }));
    _preds = extract(args, [](PhiMapping p) { return p.pred; });
}

void Phi::setArgument(BasicBlock const* pred, Value* value) {
    setArgument(indexOf(pred), value);
}

void Phi::setArgument(size_t index, Value* value) { setOperand(index, value); }

void Phi::setPredecessor(size_t index, BasicBlock* pred) {
    _preds[index] = pred;
}

void Phi::addArgument(BasicBlock* pred, Value* value) {
    _preds.push_back(pred);
    addOperand(value);
}

Value const* Phi::operandOf(BasicBlock const* pred) const {
    auto itr = ranges::find(_preds, pred);
    SC_ASSERT(itr != ranges::end(_preds),
              "`pred` is not a predecessor of this phi node");
    size_t index = utl::narrow_cast<size_t>(itr - ranges::begin(_preds));
    return operands()[index];
}

template <typename SizeT>
static Type const* computeAccessedTypeGen(Type const* operandType,
                                          std::span<SizeT const> indices) {
    Type const* result = operandType;
    for (auto index: indices) {
        // clang-format off
        result = SC_MATCH (*result) {
            [&](StructType const& type) {
                return type.memberAt(index);
            },
            [&](ArrayType const& type) {
                SC_ASSERT(index < type.count(), "Index out of bounds");
                return type.elementType();
            },
            [](Type const&) -> ir::Type const* { SC_UNREACHABLE(); }
        }; // clang-format on
    }
    return result;
}

GetElementPointer::GetElementPointer(Context& context,
                                     Type const* inboundsType,
                                     Value* basePointer,
                                     Value* arrayIndex,
                                     std::span<size_t const> memberIndices,
                                     std::string name):
    Instruction(NodeType::GetElementPointer,
                context.ptrType(),
                std::move(name),
                { basePointer, arrayIndex },
                { inboundsType }),
    _memberIndices(memberIndices | ranges::to<utl::small_vector<uint16_t>>) {}

Type const* GetElementPointer::accessedType() const {
    return computeAccessedTypeGen(inboundsType(), memberIndices());
}

bool GetElementPointer::hasConstantArrayIndex() const {
    return isa<IntegralConstant>(arrayIndex());
}

size_t GetElementPointer::constantArrayIndex() const {
    return cast<IntegralConstant const*>(arrayIndex())->value().to<size_t>();
}

Type const* ir::internal::AccessValueBase::computeAccessedType(
    Type const* operandType, std::span<size_t const> indices) {
    return computeAccessedTypeGen(operandType, indices);
}

void ExtractValue::setBaseValue(Value* value) {
    if (!type()) {
        setType(computeAccessedTypeGen(value->type(), memberIndices()));
    }
    setOperand(value);
}

InsertValue::InsertValue(Value* baseValue,
                         Value* insertedValue,
                         std::span<size_t const> indices,
                         std::string name):
    BinaryInstruction(NodeType::InsertValue,
                      baseValue,
                      insertedValue,
                      baseValue ? baseValue->type() : nullptr,
                      std::move(name)),
    internal::AccessValueBase(indices) {}

Select::Select(Value* condition,
               Value* thenValue,
               Value* elseValue,
               std::string name):
    Instruction(NodeType::Select,
                thenValue ? thenValue->type() : nullptr,
                std::move(name),
                { condition, thenValue, elseValue }) {}
