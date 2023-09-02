#include "AST/Lowering/LoweringContext.h"

#include <svm/Builtin.h>

#include "AST/AST.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Type.h"
#include "Sema/Analysis/ConstantExpressions.h"
#include "Sema/Analysis/Conversion.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace ast;
using sema::QualType;

using enum ValueLocation;

static bool isIntType(size_t width, ir::Type const* type) {
    return cast<ir::IntegralType const*>(type)->bitwidth() == 1;
}

/// MARK: getValue() Implementation

Value LoweringContext::getValue(Expression const* expr) {
    SC_ASSERT(expr, "");
    /// Returning constants here if possible breaks when we take the address of
    /// a constant. A solution that also solves the array size problem could be
    /// to add additional optional data to values (other values) that could get
    /// resovled by the `toRegister` function. I.e. when we call `getValue` on
    /// an identifier, we get a value that represents the value in memory, but
    /// is annotated with the constant. Then when we call `toRegister` on the
    /// value it checks whether the value is annotated with a constant and if so
    /// returns that. Otherwise it defaults to loading the value.
#if 0
    if (auto* constVal = expr->constantValue();
        constVal && !expr->type()->isReference())
    {
        auto* type = cast<sema::ArithmeticType
        const*>(expr->type()->base());
        // clang-format off
        return visit(*constVal, utl::overload{
            [&](sema::IntValue const& intVal) {
                SC_ASSERT(type->bitwidth() == intVal.value().bitwidth(),
                          "");
                return Value(intConstant(intVal.value()), Register);
            },
            [&](sema::FloatValue const& floatVal) {
                return Value(floatConstant(floatVal.value()), Register);
            }
        }); // clang-format on
    }
#endif
    return visit(*expr,
                 [this](auto const& expr) { return getValueImpl(expr); });
}

Value LoweringContext::getValueImpl(Identifier const& id) {
    Value value = objectMap[id.object()];
    SC_ASSERT(value, "Undeclared identifier");
    return value;
}

Value LoweringContext::getValueImpl(Literal const& lit) {
    switch (lit.kind()) {
    case LiteralKind::Integer:
        return Value(newID(), intConstant(lit.value<APInt>()), Register);
    case LiteralKind::Boolean:
        return Value(newID(), intConstant(lit.value<APInt>()), Register);
    case LiteralKind::FloatingPoint:
        return Value(newID(), floatConstant(lit.value<APFloat>()), Register);
    case LiteralKind::This: {
        return objectMap[lit.object()];
    }
    case LiteralKind::String: {
        std::string const& sourceText = lit.value<std::string>();
        size_t size = sourceText.size();
        utl::vector<u8> text(sourceText.begin(), sourceText.end());
        auto* type = ctx.arrayType(ctx.integralType(8), size);
        auto staticData =
            allocate<ir::ConstantData>(ctx, type, std::move(text), "stringlit");
        auto data = Value(newID(),
                          staticData.get(),
                          staticData.get()->type(),
                          Register,
                          sema::ValueCategory::LValue);
        mod.addConstantData(std::move(staticData));
        memorizeArraySize(data.ID(), size);
        return data;
    }
    case LiteralKind::Char:
        return Value(newID(), intConstant(lit.value<APInt>()), Register);
    }
}

Value LoweringContext::getValueImpl(UnaryExpression const& expr) {
    switch (expr.operation()) {
        using enum UnaryOperator;
    case Increment:
        [[fallthrough]];
    case Decrement: {
        Value operand = getValue(expr.operand());
        ir::Value* opAddr = toRegister(operand);
        ir::Type const* operandType =
            mapType(sema::stripReference(expr.operand()->type()));
        ir::Value* operandValue =
            add<ir::Load>(opAddr,
                          operandType,
                          utl::strcat(expr.operation(), ".op"));
        auto* newValue =
            add<ir::ArithmeticInst>(operandValue,
                                    constant(1, operandType),
                                    expr.operation() == Increment ?
                                        ir::ArithmeticOperation::Add :
                                        ir::ArithmeticOperation::Sub,
                                    utl::strcat(expr.operation(), ".res"));
        add<ir::Store>(opAddr, newValue);
        switch (expr.notation()) {
        case ast::UnaryOperatorNotation::Prefix:
            return operand;
        case ast::UnaryOperatorNotation::Postfix:
            return Value(newID(), operandValue, Register);
        case ast::UnaryOperatorNotation::_count:
            SC_UNREACHABLE();
        }
    }

    case ast::UnaryOperator::Promotion:
        return getValue(expr.operand());

    case ast::UnaryOperator::Negation: {
        auto* operand = toRegister(getValue(expr.operand()));
        auto operation = isa<sema::IntType>(expr.operand()->type().get()) ?
                             ir::ArithmeticOperation::Sub :
                             ir::ArithmeticOperation::FSub;
        auto* newValue = add<ir::ArithmeticInst>(constant(0, operand->type()),
                                                 operand,
                                                 operation,
                                                 "negated");
        return Value(newID(), newValue, Register);
    }

    default:
        auto* operand = toRegister(getValue(expr.operand()));
        auto* newValue =
            add<ir::UnaryArithmeticInst>(operand,
                                         mapUnaryOp(expr.operation()),
                                         "expr");
        return Value(newID(), newValue, Register);
    }
}

Value LoweringContext::getValueImpl(BinaryExpression const& expr) {
    auto* builtinType = dyncast<sema::BuiltinType const*>(
        sema::stripReference(expr.lhs()->type()).get());

    switch (expr.operation()) {
        using enum BinaryOperator;
    case Multiplication:
        [[fallthrough]];
    case Division:
        [[fallthrough]];
    case Remainder:
        [[fallthrough]];
    case Addition:
        [[fallthrough]];
    case Subtraction:
        [[fallthrough]];
    case LeftShift:
        [[fallthrough]];
    case RightShift:
        [[fallthrough]];
    case BitwiseAnd:
        [[fallthrough]];
    case BitwiseXOr:
        [[fallthrough]];
    case BitwiseOr: {
        auto* lhs = getValue<Register>(expr.lhs());
        auto* rhs = getValue<Register>(expr.rhs());
        auto* type = lhs->type();
        if (expr.operation() != LeftShift && expr.operation() != RightShift) {
            SC_ASSERT(lhs->type() == rhs->type(),
                      "Need same types to do arithmetic");
            SC_ASSERT(isa<ir::ArithmeticType>(type),
                      "Need arithmetic type to do arithmetic");
        }
        else {
            SC_ASSERT(isa<ir::IntegralType>(lhs->type()),
                      "Need integral type for shift");
            SC_ASSERT(isa<ir::IntegralType>(rhs->type()),
                      "Need integral type for shift");
        }
        auto operation = mapArithmeticOp(builtinType, expr.operation());
        auto* result = add<ir::ArithmeticInst>(lhs, rhs, operation, "expr");
        return Value(newID(), result, Register);
    }

    case LogicalAnd:
        [[fallthrough]];
    case LogicalOr: {
        ir::Value* const lhs = getValue<Register>(expr.lhs());
        SC_ASSERT(isIntType(1, lhs->type()), "Need i1 for logical operation");
        auto* startBlock = currentBlock;
        auto* rhsBlock = newBlock("log.rhs");
        auto* endBlock = newBlock("log.end");
        if (expr.operation() == LogicalAnd) {
            add<ir::Branch>(lhs, rhsBlock, endBlock);
        }
        else {
            add<ir::Branch>(lhs, endBlock, rhsBlock);
        }

        add(rhsBlock);
        auto* rhs = getValue<Register>(expr.rhs());
        SC_ASSERT(isIntType(1, rhs->type()), "Need i1 for logical operation");
        add<ir::Goto>(endBlock);
        add(endBlock);

        auto* result = [&] {
            if (expr.operation() == LogicalAnd) {
                return add<ir::Phi>(
                    std::array<ir::PhiMapping, 2>{
                        ir::PhiMapping{ startBlock, intConstant(0, 1) },
                        ir::PhiMapping{ rhsBlock, rhs } },
                    "log.and");
            }
            else {
                return add<ir::Phi>(
                    std::array<ir::PhiMapping, 2>{
                        ir::PhiMapping{ startBlock, intConstant(1, 1) },
                        ir::PhiMapping{ rhsBlock, rhs } },
                    "log.or");
            }
        }();
        return Value(newID(), result, Register);
    }

    case Less:
        [[fallthrough]];
    case LessEq:
        [[fallthrough]];
    case Greater:
        [[fallthrough]];
    case GreaterEq:
        [[fallthrough]];
    case Equals:
        [[fallthrough]];
    case NotEquals: {
        auto* result = add<ir::CompareInst>(getValue<Register>(expr.lhs()),
                                            getValue<Register>(expr.rhs()),
                                            mapCompareMode(builtinType),
                                            mapCompareOp(expr.operation()),
                                            "cmp.res");
        return Value(newID(), result, Register);
    }

    case Comma:
        getValue(expr.lhs());
        return getValue(expr.rhs());

    case Assignment:
        [[fallthrough]];
    case AddAssignment:
        [[fallthrough]];
    case SubAssignment:
        [[fallthrough]];
    case MulAssignment:
        [[fallthrough]];
    case DivAssignment:
        [[fallthrough]];
    case RemAssignment:
        [[fallthrough]];
    case LSAssignment:
        [[fallthrough]];
    case RSAssignment:
        [[fallthrough]];
    case AndAssignment:
        [[fallthrough]];
    case OrAssignment:
        [[fallthrough]];
    case XOrAssignment: {
        auto lhs = getValue(expr.lhs());
        auto rhs = getValue(expr.rhs());
        auto lhsReg = toRegister(lhs);
        auto rhsReg = toRegister(rhs);
        if (expr.operation() != Assignment) {
            SC_ASSERT(builtinType == expr.rhs()->type().get(), "");
            auto* lhsValue = add<ir::Load>(lhsReg, mapType(builtinType), "lhs");
            auto operation =
                mapArithmeticAssignOp(builtinType, expr.operation());
            rhsReg =
                add<ir::ArithmeticInst>(lhsValue, rhsReg, operation, "expr");
        }
        add<ir::Store>(lhsReg, rhsReg);
        if (auto* arrayType = dyncast<sema::ArrayType const*>(
                sema::stripReference(expr.lhs()->type()).get());
            arrayType && arrayType->isDynamic())
        {
            SC_ASSERT(expr.operation() == Assignment, "");
            SC_ASSERT(sema::isRef(expr.lhs()->type()), "");
            auto lhsSize = getArraySize(lhs.ID());
            SC_ASSERT(lhsSize.location() == Memory,
                      "Must be in memory to reassign");
            auto* rhsSizeReg = toRegister(getArraySize(rhs.ID()));
            add<ir::Store>(lhsSize.get(), rhsSizeReg);
        }
        return Value();
    }
    case BinaryOperator::_count:
        SC_UNREACHABLE();
    }
}

Value LoweringContext::getValueImpl(MemberAccess const& expr) {
    if (auto itr = valueMap.find(expr.member()->entity());
        itr != valueMap.end())
    {
        return itr->second;
    }
    if (auto* arrayType =
            dyncast<sema::ArrayType const*>(expr.object()->type().get()))
    {
        SC_ASSERT(expr.member()->value() == "count", "What else?");
        auto value = getValue(expr.object());
        return getArraySize(value.ID());
    }

    Value base = getValue(expr.object());
    auto* accessedId = cast<Identifier const*>(expr.member());
    auto* var = cast<sema::Variable const*>(accessedId->entity());

    Value value;
    size_t const irIndex = structIndexMap[{
        cast<sema::StructureType const*>(expr.object()->type().get()),
        var->index() }];
    switch (base.location()) {
    case Register: {
        auto* result =
            add<ir::ExtractValue>(base.get(), std::array{ irIndex }, "mem.acc");
        value = Value(newID(), result, Register);
        break;
    }
    case Memory: {
        auto* baseType = mapType(sema::stripReference(expr.object()->type()));
        auto* result = add<ir::GetElementPointer>(baseType,
                                                  base.get(),
                                                  intConstant(0, 64),
                                                  std::array{ irIndex },
                                                  "mem.acc");
        auto* accessedType = mapType(var->type());
        value = Value(newID(), result, accessedType, Memory);
        break;
    }
    }
    QualType memType = expr.type();
    auto* arrayType =
        dyncast<sema::ArrayType const*>(stripReference(memType).get());
    if (!arrayType) {
        return value;
    }
    Value size;
    switch (base.location()) {
    case Register: {
        auto* result = add<ir::ExtractValue>(base.get(),
                                             std::array{ irIndex + 1 },
                                             "mem.acc.size");
        size = Value(newID(), result, Register);
        break;
    }
    case Memory: {
        auto* baseType = mapType(sema::stripReference(expr.object()->type()));
        auto* result = add<ir::GetElementPointer>(baseType,
                                                  base.get(),
                                                  intConstant(0, 64),
                                                  std::array{ irIndex + 1 },
                                                  "mem.acc.size");
        auto* accessedType = mapType(var->type());
        size = Value(newID(), result, accessedType, Memory);
        break;
    }
    }
    memorizeArraySize(value.ID(), size);
    return value;
}

Value LoweringContext::getValueImpl(ReferenceExpression const& expr) {
    auto* referred = expr.referred();
    auto value = getValue(referred);
    if (sema::isRef(referred->type())) {
        return value;
    }
    SC_ASSERT(value.isMemory(), "Can only take references to values in memory");
    return Value(value.ID(), value.get(), Register);
}

Value LoweringContext::getValueImpl(UniqueExpression const& expr) {
    SC_UNIMPLEMENTED();
}

Value LoweringContext::getValueImpl(Conditional const& condExpr) {
    auto* cond = getValue<Register>(condExpr.condition());
    auto* thenBlock = newBlock("cond.then");
    auto* elseBlock = newBlock("cond.else");
    auto* endBlock = newBlock("cond.end");
    add<ir::Branch>(cond, thenBlock, elseBlock);

    /// Generate then block.
    add(thenBlock);
    auto* thenVal = getValue<Register>(condExpr.thenExpr());
    thenBlock = currentBlock; /// Nested `?:` operands etc. may have changed
                              /// `currentBlock`
    add<ir::Goto>(endBlock);

    /// Generate else block.
    add(elseBlock);
    auto* elseVal = getValue<Register>(condExpr.elseExpr());
    elseBlock = currentBlock;
    add<ir::Goto>(endBlock);

    /// Generate end block.
    add(endBlock);
    std::array<ir::PhiMapping, 2> phiArgs = { { { thenBlock, thenVal },
                                                { elseBlock, elseVal } } };
    auto* result = add<ir::Phi>(phiArgs, "cond");
    return Value(newID(), result, Register);
}

Value LoweringContext::getValueImpl(FunctionCall const& call) {
    ir::Callable* function = getFunction(call.function());
    auto CC = CCMap[call.function()];
    utl::small_vector<ir::Value*> arguments;
    auto const retvalLocation = CC.returnValue().location();
    if (retvalLocation == Memory) {
        auto* returnType = mapType(call.function()->returnType());
        arguments.push_back(makeLocal(returnType, "retval"));
    }
    for (auto [PC, arg]: ranges::views::zip(CC.arguments(), call.arguments())) {
        generateArgument(PC, getValue(arg), arguments);
    }
    bool callHasName = !isa<ir::VoidType>(function->returnType());
    std::string name = callHasName ? "call.result" : std::string{};
    auto* inst = add<ir::Call>(function, arguments, std::move(name));

    switch (sema::stripReference(call.type())->entityType()) {
    case sema::EntityType::ArrayType: {
        switch (retvalLocation) {
        case Register: {
            auto* data =
                add<ir::ExtractValue>(inst, std::array{ size_t{ 0 } }, "data");
            auto* size =
                add<ir::ExtractValue>(inst, std::array{ size_t{ 1 } }, "size");
            Value value(newID(), data, Register);
            memorizeArraySize(value.ID(), Value(newID(), size, Register));
            return value;
        }
        case Memory:
            SC_UNIMPLEMENTED();
        }
    }
    default:
        switch (retvalLocation) {
        case Register:
            return Value(newID(), inst, Register);
        case Memory:
            return Value(newID(),
                         arguments.front(),
                         mapType(call.function()->returnType()),
                         Memory,
                         sema::ValueCategory::RValue);
        }
        break;
    }
}

void LoweringContext::generateArgument(PassingConvention const& PC,
                                       Value value,
                                       utl::vector<ir::Value*>& arguments) {
    switch (PC.location()) {
    case Register:
        arguments.push_back(toRegister(value));
        break;

    case Memory:
        if (value.isLValue()) {
            auto* reg = toRegister(value);
            arguments.push_back(
                storeLocal(reg, utl::strcat(value.get()->name(), ".param")));
        }
        else {
            arguments.push_back(toMemory(value));
        }
        break;
    }
    if (PC.numParams() == 2) {
        arguments.push_back(toRegister(getArraySize(value.ID())));
    }
}

Value LoweringContext::getValueImpl(Subscript const& expr) {
    auto* arrayType = cast<sema::ArrayType const*>(
        stripReference(expr.object()->type()).get());
    auto* elemType = mapType(arrayType->elementType());
    auto array = getValue(expr.object());
    /// Right now we don't use the size but here we could at a call to an
    /// assertion function
    [[maybe_unused]] auto size = getArraySize(array.ID());
    auto index = getValue<Register>(expr.arguments().front());
    switch (array.location()) {
    case Register:
        /// Here we should insert `ExtractValue` instruction
        SC_UNIMPLEMENTED();
    case Memory: {
        auto* addr = add<ir::GetElementPointer>(elemType,
                                                array.get(),
                                                index,
                                                std::initializer_list<size_t>{},
                                                "elem.ptr");
        return Value(newID(), addr, elemType, Register, array.valueCategory());
    }
    }
}

static bool evalConstant(Expression const* expr, utl::vector<u8>& dest) {
    auto* val = dyncast_or_null<sema::IntValue const*>(expr->constantValue());
    if (!val) {
        return false;
    }
    auto value = val->value();
    size_t const elemSize = expr->type()->size();
    auto* data = reinterpret_cast<u8 const*>(value.limbs().data());
    for (auto* end = data + elemSize; data < end; ++data) {
        dest.push_back(*data);
    }
    return true;
}

bool LoweringContext::genStaticListData(ListExpression const& list,
                                        ir::Alloca* dest) {
    auto* type = cast<sema::ArrayType const*>(list.type().get());
    auto* elemType = type->elementType();
    utl::small_vector<u8> data;
    data.reserve(type->size());
    for (auto* expr: list.elements()) {
        SC_ASSERT(elemType == expr->type(), "Invalid type");
        if (!evalConstant(expr, data)) {
            return false;
        }
    }
    auto constData =
        allocate<ir::ConstantData>(ctx,
                                   ctx.arrayType(mapType(elemType),
                                                 list.elements().size()),
                                   std::move(data),
                                   "array");
    auto* source = constData.get();
    mod.addConstantData(std::move(constData));
    auto* memcpy = getFunction(
        symbolTable.builtinFunction(static_cast<size_t>(svm::Builtin::memcpy)));
    auto* size = intConstant(list.elements().size() * elemType->size(), 64);
    std::array<ir::Value*, 4> args = { dest, size, source, size };
    add<ir::Call>(memcpy, args, std::string{});
    return true;
}

void LoweringContext::genListDataFallback(ListExpression const& list,
                                          ir::Alloca* dest) {
    auto* arrayType = cast<sema::ArrayType const*>(list.type().get());
    auto* elemType = mapType(arrayType->elementType());
    for (auto [index, elem]: list.elements() | ranges::views::enumerate) {
        auto* gep = add<ir::GetElementPointer>(elemType,
                                               dest,
                                               intConstant(index, 32),
                                               std::initializer_list<size_t>{},
                                               "elem.ptr");
        add<ir::Store>(gep, getValue<Register>(elem));
    }
}

Value LoweringContext::getValueImpl(ListExpression const& list) {
    auto* arrayType = cast<sema::ArrayType const*>(list.type().get());
    auto* elemType = arrayType->elementType();
    auto* array = new ir::Alloca(ctx,
                                 intConstant(arrayType->count(), 32),
                                 mapType(elemType),
                                 "list");
    allocas.push_back(array);
    Value size(newID(), intConstant(list.children().size(), 64), Register);
    valueMap.insert({ arrayType->countVariable(), size });
    auto value = Value(newID(),
                       array,
                       mapType(arrayType),
                       Memory,
                       sema::ValueCategory::RValue);
    if (!genStaticListData(list, array)) {
        genListDataFallback(list, array);
    }
    memorizeArraySize(value.ID(), size);
    return value;
}

Value LoweringContext::getValueImpl(Conversion const& conv) {
    auto* expr = conv.expression();
    Value refConvResult = [&]() -> Value {
        switch (conv.conversion()->refConversion()) {
        case sema::RefConversion::None:
            return getValue(expr);

        case sema::RefConversion::Dereference:
            [[fallthrough]];
        case sema::RefConversion::DerefExpl: {
            auto address = getValue(expr);
            return Value(address.ID(),
                         toRegister(address),
                         mapType(stripReference(expr->type())),
                         Memory);
        }

        case sema::RefConversion::TakeAddress: {
            auto value = getValue(expr);
            SC_ASSERT(value.isMemory(), "");
            return Value(value.ID(), value.get(), Register);
        }
        }
    }();

    switch (conv.conversion()->objectConversion()) {
        using enum sema::ObjectTypeConversion;
    case None:
        return refConvResult;

    case Array_FixedToDynamic: {
        return refConvResult;
    }
    case Reinterpret_ArrayRef_ToByte:
        [[fallthrough]];
    case Reinterpret_ArrayRef_FromByte: {
        SC_ASSERT(sema::isRef(expr->type()), "");
        SC_ASSERT(sema::isRef(conv.type()), "");
        auto* fromType = cast<sema::ArrayType const*>(
            sema::stripReference(expr->type()).get());
        auto* toType = cast<sema::ArrayType const*>(
            sema::stripReference(conv.type()).get());
        auto data = refConvResult;
        if (toType->isDynamic()) {
            uint32_t const oldID = data.ID();
            data.setID(newID());
            if (fromType->isDynamic()) {
                auto count = getArraySize(oldID);
                if (conv.conversion()->objectConversion() ==
                    Reinterpret_ArrayRef_ToByte)
                {
                    auto* newCount =
                        add<ir::ArithmeticInst>(toRegister(count),
                                                intConstant(8, 64),
                                                ir::ArithmeticOperation::Mul,
                                                "reinterpret.count");
                    count = Value(newID(), newCount, Register);
                }
                else {
                    auto* newCount =
                        add<ir::ArithmeticInst>(toRegister(count),
                                                intConstant(8, 64),
                                                ir::ArithmeticOperation::SDiv,
                                                "reinterpret.count");
                    count = Value(newID(), newCount, Register);
                }
                memorizeArraySize(data.ID(), count);
                return data;
            }
            size_t count = fromType->count();
            switch (conv.conversion()->objectConversion()) {
            case Reinterpret_ArrayRef_ToByte:
                count *= 8;
                break;
            case Reinterpret_ArrayRef_FromByte:
                count /= 8;
                break;
            default:
                SC_UNREACHABLE();
            }
            memorizeArraySize(data.ID(), count);
            return data;
        }
        SC_ASSERT(!fromType->isDynamic(), "Invalid conversion");
        return data;
    }

    case Reinterpret_Value: {
        auto* result = add<ir::ConversionInst>(toRegister(refConvResult),
                                               mapType(conv.type()),
                                               ir::Conversion::Bitcast,
                                               "reinterpret");
        return Value(newID(), result, Register);
    }
    case SS_Trunc:
        [[fallthrough]];
    case SU_Trunc:
        [[fallthrough]];
    case US_Trunc:
        [[fallthrough]];
    case UU_Trunc: {
        auto* result = add<ir::ConversionInst>(toRegister(refConvResult),
                                               mapType(conv.type()),
                                               ir::Conversion::Trunc,
                                               "trunc");
        return Value(newID(), result, Register);
    }
    case SS_Widen:
        [[fallthrough]];
    case SU_Widen: {
        auto* result = add<ir::ConversionInst>(toRegister(refConvResult),
                                               mapType(conv.type()),
                                               ir::Conversion::Sext,
                                               "sext");
        return Value(newID(), result, Register);
    }
    case US_Widen:
        [[fallthrough]];
    case UU_Widen: {
        auto* result = add<ir::ConversionInst>(toRegister(refConvResult),
                                               mapType(conv.type()),
                                               ir::Conversion::Zext,
                                               "zext");
        return Value(newID(), result, Register);
    }
    case Float_Trunc: {
        auto* result = add<ir::ConversionInst>(toRegister(refConvResult),
                                               mapType(conv.type()),
                                               ir::Conversion::Ftrunc,
                                               "ftrunc");
        return Value(newID(), result, Register);
    }
    case Float_Widen: {
        auto* result = add<ir::ConversionInst>(toRegister(refConvResult),
                                               mapType(conv.type()),
                                               ir::Conversion::Fext,
                                               "fext");
        return Value(newID(), result, Register);
    }
    case SignedToFloat: {
        auto* result = add<ir::ConversionInst>(toRegister(refConvResult),
                                               mapType(conv.type()),
                                               ir::Conversion::StoF,
                                               "stof");
        return Value(newID(), result, Register);
    }
    case UnsignedToFloat: {
        auto* result = add<ir::ConversionInst>(toRegister(refConvResult),
                                               mapType(conv.type()),
                                               ir::Conversion::UtoF,
                                               "utof");
        return Value(newID(), result, Register);
    }
    case FloatToSigned: {
        auto* result = add<ir::ConversionInst>(toRegister(refConvResult),
                                               mapType(conv.type()),
                                               ir::Conversion::FtoS,
                                               "ftos");
        return Value(newID(), result, Register);
    }
    case FloatToUnsigned: {
        auto* result = add<ir::ConversionInst>(toRegister(refConvResult),
                                               mapType(conv.type()),
                                               ir::Conversion::FtoU,
                                               "ftou");
        return Value(newID(), result, Register);
    }
    }
}

Value LoweringContext::getValueImpl(ConstructorCall const& call) {
    using enum sema::SpecialMemberFunction;
    switch (call.kind()) {
    case New: {
        auto* type = mapType(call.constructedType());
        auto* address = makeLocal(type, "anon");
        auto* function = getFunction(call.function());
        auto CC = CCMap[call.function()];
        /// Lifetime function always must take the object parameter by reference
        /// so we can just pass the pointer here
        utl::small_vector<ir::Value*> arguments = { address };
        for (auto [PC, arg]:
             ranges::views::zip(CC.arguments(), call.arguments()))
        {
            generateArgument(PC, getValue(arg), arguments);
        }
        memorizeObject(call.object(), Value(newID(), address, type, Memory));
        add<ir::Call>(function, arguments, std::string{});
        return Value(newID(),
                     address,
                     type,
                     Memory,
                     sema::ValueCategory::RValue);
    }
    case Move:
        SC_UNIMPLEMENTED();
    default:
        SC_UNREACHABLE();
    }
}
