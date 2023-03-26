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

void Value::setName(std::string name) {
    auto makeUnique = [&](std::string& name, Function& func) {
        func.nameFac.tryErase(_name);
        name = func.nameFac.makeUnique(std::move(name));
    };
    // clang-format off
    visit(*this, utl::overload{
        [&](BasicBlock& bb) {
            auto* func = bb.parent();
            if (func) {
                makeUnique(name, *func);
            }
        },
        [&](Instruction& inst) {
            auto* bb   = inst.parent();
            auto* func = bb ? bb->parent() : nullptr;
            if (func) {
                makeUnique(name, *func);
            }
        },
        [](Value const&) {}
    }); // clang-format on
    _name = std::move(name);
}

void Value::uniqueExistingName(Function& func) {
    _name = func.nameFac.makeUnique(std::move(_name));
}

void scatha::internal::privateDelete(Value* value) {
    visit(*value, [](auto& derived) { delete &derived; });
}

void scatha::internal::privateDestroy(Value* value) {
    visit(*value, [](auto& derived) { std::destroy_at(&derived); });
}

User::User(NodeType nodeType,
           Type const* type,
           std::string name,
           utl::small_vector<Value*> operands):
    Value(nodeType, type, std::move(name)) {
    setOperands(std::move(operands));
}

void User::setOperand(size_t index, Value* operand) {
    SC_ASSERT(index < _operands.size(),
              "`index` not valid for this instruction");
    if (auto* op = _operands[index]) {
        op->removeUserWeak(this);
    }
    if (operand) {
        operand->addUserWeak(this);
    }
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
    bool leastOne = false;
    for (auto&& [index, op]: _operands | ranges::views::enumerate) {
        if (op == oldOperand) {
            setOperand(index, newOperand);
            leastOne = true;
        }
    }
    SC_ASSERT(leastOne, "Not found");
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

bool User::directlyUses(Value const* value) const {
    return ranges::find(_operands, value) != ranges::end(_operands);
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

void Instruction::setTypeOperand(size_t index, Type const* type) {
    SC_ASSERT(index < typeOps.size(), "Invalid index");
    typeOps[index] = type;
}

BasicBlock::BasicBlock(Context& context, std::string name):
    Value(NodeType::BasicBlock, context.voidType(), std::move(name)) {}

void BasicBlock::eraseAllPhiNodes() {
    auto phiEnd = begin();
    while (isa<Phi>(*phiEnd)) {
        ++phiEnd;
    }
    erase(begin(), phiEnd);
}

bool BasicBlock::contains(Instruction const& inst) const {
    auto const itr = std::find_if(begin(), end(), [&](Instruction const& i) {
        return &i == &inst;
    });
    return itr != end();
}

bool BasicBlock::emptyExceptTerminator() const {
    return terminator() == &front();
}

void BasicBlock::updatePredecessor(BasicBlock const* oldPred,
                                   BasicBlock* newPred) {
    auto itr = ranges::find(preds, oldPred);
    SC_ASSERT(itr != ranges::end(preds), "Not found");
    *itr = newPred;
    for (auto& phi: phiNodes()) {
        size_t const index = phi.indexOf(oldPred);
        phi.setPredecessor(index, newPred);
    }
}

void BasicBlock::removePredecessor(BasicBlock const* pred) {
    auto itr   = ranges::find(preds, pred);
    auto index = utl::narrow_cast<size_t>(itr - ranges::begin(preds));
    removePredecessor(index);
}

void BasicBlock::removePredecessor(size_t index) {
    SC_ASSERT(index < preds.size(), "");
    auto itr   = preds.begin() + index;
    auto* pred = *itr;
    preds.erase(itr);
    for (auto& phi: phiNodes()) {
        phi.removeArgument(pred);
    }
}

void BasicBlock::insertCallback(Instruction& inst) {
    inst.set_parent(this);
    if (auto* func = parent()) {
        inst.uniqueExistingName(*func);
    }
}

void BasicBlock::eraseCallback(Instruction const& inst) {
    const_cast<Instruction&>(inst).clearOperands();
    parent()->nameFac.erase(inst.name());
}

bool BasicBlock::isEntry() const {
    return parent()->begin().to_address() == this;
}

static auto phiEndImpl(auto begin, auto end) {
    while (begin != end && isa<Phi>(begin.to_address())) {
        ++begin;
    }
    return begin;
}

BasicBlock::Iterator BasicBlock::phiEnd() { return phiEndImpl(begin(), end()); }

BasicBlock::ConstIterator BasicBlock::phiEnd() const {
    return phiEndImpl(begin(), end());
}

Callable::Callable(NodeType nodeType,
                   FunctionType const* functionType,
                   Type const* returnType,
                   std::span<Type const* const> parameterTypes,
                   std::string name,
                   FunctionAttribute attr):
    Constant(nodeType, functionType, std::move(name)),
    _returnType(returnType),
    attrs(attr) {
    for (auto [index, type]: parameterTypes | ranges::views::enumerate) {
        params.emplace_back(type, index++, this);
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
    for (auto& param: parameters()) {
        bool const nameUnique = nameFac.tryRegister(param.name());
        SC_ASSERT(nameUnique, "How are the parameter names not unique?");
    }
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

Return::Return(Context& context): Return(context, context.voidValue()) {}

Alloca::Alloca(Context& context, Type const* allocatedType, std::string name):
    Instruction(NodeType::Alloca,
                context.pointerType(),
                std::move(name),
                {},
                { allocatedType }) {}

void Load::setAddress(Value* address) {
    SC_ASSERT(isa<PointerType>(address->type()),
              "`address` must be of type `ptr`");
    setOperand(0, address);
}

Store::Store(Context& context, Value* address, Value* value):
    Instruction(NodeType::Store,
                context.voidType(),
                std::string{},
                { address, value }) {
    SC_ASSERT(isa<PointerType>(address->type()),
              "`address` must be of type `ptr`");
}

void Store::setAddress(Value* address) {
    SC_ASSERT(isa<PointerType>(address->type()),
              "`address` must be of type `ptr`");
    setOperand(0, address);
}

void Store::setValue(Value* value) { setOperand(1, value); }

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
    default:
        SC_UNREACHABLE();
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

Call::Call(Callable* function,
           std::span<Value* const> arguments,
           std::string name):
    Instruction(NodeType::Call, function->returnType(), std::move(name)) {
    utl::small_vector<Value*> args;
    args.reserve(1 + arguments.size());
    args.push_back(function);
    ranges::copy(arguments, std::back_inserter(args));
    setOperands(std::move(args));
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
    SC_ASSERT(!args.empty(), "Phi must have at least one argument");
    setType(args[0].value->type());
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
        result = cast<StructureType const*>(result)->memberAt(index);
    }
    return result;
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

Type const* GetElementPointer::accessedType() const {
    return computeAccessedTypeGen(inboundsType(), memberIndices());
}

Type const* ir::internal::AccessValueBase::computeAccessedType(
    Type const* operandType, std::span<size_t const> indices) {
    return computeAccessedTypeGen(operandType, indices);
}

InsertValue::InsertValue(Value* baseValue,
                         Value* insertedValue,
                         std::span<size_t const> indices,
                         std::string name):
    BinaryInstruction(NodeType::InsertValue,
                      baseValue,
                      insertedValue,
                      baseValue->type(),
                      std::move(name)),
    internal::AccessValueBase(indices) {
    auto* compAccessedType = computeAccessedType(baseValue->type(), indices);
    SC_ASSERT(insertedValue->type() == compAccessedType, "Type mismatch");
}

Select::Select(Value* condition,
               Value* thenValue,
               Value* elseValue,
               std::string name):
    Instruction(NodeType::Select,
                thenValue->type(),
                std::move(name),
                { condition, thenValue, elseValue }) {
    SC_ASSERT(thenValue->type() == elseValue->type(), "Type mismatch");
    SC_ASSERT(cast<IntegralType const*>(condition->type())->bitWidth() == 1,
              "`condition` needs to be of type i1");
}
