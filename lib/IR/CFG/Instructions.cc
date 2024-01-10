#include "IR/CFG/Instructions.h"

#include <range/v3/algorithm.hpp>

#include "IR/CFG.h"
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

bool Alloca::isStatic() const { return isa<IntegralConstant>(count()); }

std::optional<size_t> Alloca::constantCount() const {
    auto* constCount = dyncast<IntegralConstant const*>(count());
    if (!constCount) {
        return std::nullopt;
    }
    return constCount->value().to<size_t>();
}

std::optional<size_t> Alloca::allocatedSize() const {
    if (auto count = constantCount()) {
        return allocatedType()->size() * *count;
    }
    return std::nullopt;
}

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

BasicBlock const* TerminatorInst::targetAt(size_t index) const {
    return targets()[utl::narrow_cast<ssize_t>(index)];
}

void TerminatorInst::updateTarget(BasicBlock* oldTarget,
                                  BasicBlock* newTarget) {
    updateOperand(oldTarget, newTarget);
}

void TerminatorInst::mapTargets(
    utl::function_view<BasicBlock*(BasicBlock*)> op) {
    for (auto [index, target]: targets() | ranges::views::enumerate) {
        setTarget(index, op(target));
    }
}

void TerminatorInst::setTarget(size_t index, BasicBlock* bb) {
    setOperand(nonTargetArguments + index, bb);
}

Return::Return(Context& context): Return(context, context.voidValue()) {}

Call::Call(Type const* returnType, Value* function, std::string name):
    Call(returnType, function, std::span<Value* const>{}, std::move(name)) {}

Call::Call(Type const* returnType,
           Value* function,
           std::span<Value* const> arguments,
           std::string name):
    Instruction(NodeType::Call, returnType, std::move(name)) {
    utl::small_vector<Value*> args;
    args.reserve(1 + arguments.size());
    args.push_back(function);
    ranges::copy(arguments, std::back_inserter(args));
    setOperands(std::move(args));
}

Call::Call(Callable* function,
           std::span<Value* const> arguments,
           std::string name):
    Call(function->returnType(), function, arguments, std::move(name)) {}

void Call::setFunction(Value* function) { setOperand(0, function); }

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
    setArgument(predIndexOf(pred), value);
}

void Phi::setArgument(size_t index, Value* value) { setOperand(index, value); }

void Phi::setPredecessor(size_t index, BasicBlock* pred) {
    _preds[index] = pred;
}

void Phi::mapPredecessors(utl::function_view<BasicBlock*(BasicBlock*)> op) {
    for (auto& pred: _preds) {
        pred = op(pred);
    }
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
    return operandAt(index);
}

BasicBlock const* Phi::predecessorOf(Value const* value) const {
    if (auto index = indexOf(value)) {
        return _preds[*index];
    }
    return nullptr;
}

Select::Select(Value* condition,
               Value* thenValue,
               Value* elseValue,
               std::string name):
    Instruction(NodeType::Select,
                thenValue ? thenValue->type() : nullptr,
                std::move(name),
                { condition, thenValue, elseValue }) {}

/// Computes the inner type and byte offset by \p indices on \p operandType
static std::pair<Type const*, size_t> computeAccessedTypeAndOffset(
    Type const* operandType, std::span<size_t const> indices) {
    Type const* type = operandType;
    size_t offset = 0;
    for (auto index: indices) {
        auto* record = cast<RecordType const*>(type);
        offset += record->offsetAt(index);
        type = record->elementAt(index);
    }
    return { type, offset };
}

void AccessValueInst::setMemberIndices(std::span<size_t const> indices) {
    _indices = indices | ToSmallVector<>;
}

GetElementPointer::GetElementPointer(Context& context,
                                     Type const* inboundsType,
                                     Value* basePointer,
                                     Value* arrayIndex,
                                     std::span<size_t const> memberIndices,
                                     std::string name):
    AccessValueInst(NodeType::GetElementPointer,
                    context.ptrType(),
                    std::move(name),
                    { basePointer,
                      arrayIndex ? arrayIndex : context.intConstant(0, 32) },
                    { inboundsType }) {
    setMemberIndices(memberIndices);
}

Type const* GetElementPointer::accessedType() const {
    return computeAccessedTypeAndOffset(inboundsType(), memberIndices()).first;
}

bool GetElementPointer::hasConstantArrayIndex() const {
    return isa<IntegralConstant>(arrayIndex());
}

std::optional<size_t> GetElementPointer::constantArrayIndex() const {
    if (auto* constIndex = dyncast<IntegralConstant const*>(arrayIndex())) {
        return constIndex->value().to<size_t>();
    }
    return std::nullopt;
}

std::optional<size_t> GetElementPointer::constantByteOffset() const {
    if (auto index = constantArrayIndex()) {
        return *index * inboundsType()->size() + innerByteOffset();
    }
    return std::nullopt;
}

size_t GetElementPointer::innerByteOffset() const {
    return computeAccessedTypeAndOffset(inboundsType(), memberIndices()).second;
}

static Type const* tryGetAccessedType(Value const* value,
                                      std::span<size_t const> indices) {
    return value ? computeAccessedTypeAndOffset(value->type(), indices).first :
                   nullptr;
}

ExtractValue::ExtractValue(Value* baseValue,
                           std::span<size_t const> indices,
                           std::string name):
    AccessValueInst(NodeType::ExtractValue,
                    tryGetAccessedType(baseValue, indices),
                    std::move(name),
                    { baseValue }) {
    setMemberIndices(indices);
}

void ExtractValue::setBaseValue(Value* value) {
    setOperand(0, value);
    setType(tryGetAccessedType(value, memberIndices()));
}

InsertValue::InsertValue(Value* baseValue,
                         Value* insertedValue,
                         std::span<size_t const> indices,
                         std::string name):
    AccessValueInst(NodeType::InsertValue,
                    baseValue ? baseValue->type() : nullptr,
                    std::move(name),
                    { baseValue, insertedValue }) {
    setMemberIndices(indices);
}

void InsertValue::setBaseValue(Value* value) {
    if (!type()) {
        setType(value->type());
    }
    setOperand(0, value);
}
