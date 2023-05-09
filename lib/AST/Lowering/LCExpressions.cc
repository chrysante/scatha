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

static bool isArray(Expression const* expr) {
    return expr->type() && isa<sema::ArrayType>(expr->type()->base());
}

static bool isIntType(size_t width, ir::Type const* type) {
    return cast<ir::IntegralType const*>(type)->bitwidth() == 1;
}

/// MARK: getValue() Implementation

ir::Value* LoweringContext::getValue(Expression const* expr) {
    if (auto* constVal = expr->constantValue();
        constVal && !expr->type()->isReference())
    {
        auto* type = cast<sema::ArithmeticType const*>(expr->type()->base());
        // clang-format off
        return visit(*constVal, utl::overload{
            [&](sema::IntValue const& intVal) {
                SC_ASSERT(type->bitwidth() == intVal.value().bitwidth(), "");
                return intConstant(intVal.value());
            },
            [&](sema::FloatValue const& floatVal) {
                return floatConstant(floatVal.value());
            }
        }); // clang-format on
    }
    return visit(*expr,
                 [this](auto const& expr) { return getValueImpl(expr); });
}

ir::Value* LoweringContext::getValueImpl(Identifier const& id) {
    return add<ir::Load>(getAddressImpl(id), mapType(id.type()), id.value());
}

ir::Value* LoweringContext::getValueImpl(Literal const& lit) {
    switch (lit.kind()) {
    case LiteralKind::Integer:
        return intConstant(lit.value<APInt>());
    case LiteralKind::Boolean:
        return intConstant(lit.value<APInt>());
    case LiteralKind::FloatingPoint:
        return floatConstant(lit.value<APFloat>());
    case LiteralKind::This: {
        return variableAddressMap[lit.entity()];
    }
    case LiteralKind::String: {
        auto const& sourceText = std::get<std::string>(lit.value());
        size_t const size      = sourceText.size();
        utl::vector<u8> text(sourceText.begin(), sourceText.end());
        auto* type = ctx.arrayType(ctx.integralType(8), size);
        auto staticData =
            allocate<ir::ConstantData>(ctx, type, std::move(text), "stringlit");
        auto* dataPtr = staticData.get();
        mod.addConstantData(std::move(staticData));
        return makeArrayRef(dataPtr, size);
    }
    }
}

ir::Value* LoweringContext::getValueImpl(UnaryExpression const& expr) {
    switch (expr.operation()) {
        using enum UnaryOperator;
    case Increment:
        [[fallthrough]];
    case Decrement: {
        ir::Value* const operandAddress = getValue(expr.operand());
        SC_ASSERT(isa<ir::PointerType>(operandAddress->type()),
                  "We need a pointer to store to");
        ir::Type const* operandType = mapType(expr.operand()->type()->base());
        ir::Value* operandValue =
            add<ir::Load>(operandAddress,
                          operandType,
                          utl::strcat(expr.operation(), ".operand"));
        auto* newValue =
            add<ir::ArithmeticInst>(operandValue,
                                    constant(1, operandType),
                                    expr.operation() == Increment ?
                                        ir::ArithmeticOperation::Add :
                                        ir::ArithmeticOperation::Sub,
                                    utl::strcat(expr.operation(), ".result"));
        add<ir::Store>(operandAddress, newValue);
        return newValue;
    }

    case ast::UnaryOperator::Promotion:
        return getValue(expr.operand());

    case ast::UnaryOperator::Negation: {
        auto* operand  = getValue(expr.operand());
        auto operation = isa<ir::IntegralType>(operand->type()) ?
                             ir::ArithmeticOperation::Sub :
                             ir::ArithmeticOperation::FSub;
        return add<ir::ArithmeticInst>(constant(0, operand->type()),
                                       operand,
                                       operation,
                                       "negated");
    }

    default:
        return add<ir::UnaryArithmeticInst>(getValue(expr.operand()),
                                            mapUnaryOp(expr.operation()),
                                            "expr.result");
    }
}

ir::Value* LoweringContext::getValueImpl(BinaryExpression const& expr) {
    auto* builtinType =
        dyncast<sema::BuiltinType const*>(expr.lhs()->type()->base());

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
        ir::Value* const lhs = getValue(expr.lhs());
        ir::Value* const rhs = getValue(expr.rhs());
        auto* type           = lhs->type();
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
        return add<ir::ArithmeticInst>(lhs, rhs, operation, "expr.result");
    }

    case LogicalAnd:
        [[fallthrough]];
    case LogicalOr: {
        ir::Value* const lhs = getValue(expr.lhs());
        SC_ASSERT(isIntType(1, lhs->type()), "Need i1 for logical operation");
        auto* startBlock = currentBlock;
        auto* rhsBlock   = newBlock("log.rhs");
        auto* endBlock   = newBlock("log.end");
        if (expr.operation() == LogicalAnd) {
            add<ir::Branch>(lhs, rhsBlock, endBlock);
        }
        else {
            add<ir::Branch>(lhs, endBlock, rhsBlock);
        }

        add(rhsBlock);
        auto* rhs = getValue(expr.rhs());
        SC_ASSERT(isIntType(1, rhs->type()), "Need i1 for logical operation");
        add<ir::Goto>(endBlock);
        add(endBlock);

        return [&] {
            if (expr.operation() == LogicalAnd) {
                return add<ir::Phi>(
                    std::array<ir::PhiMapping, 2>{
                        ir::PhiMapping{ startBlock, intConstant(0, 1) },
                        ir::PhiMapping{ rhsBlock, rhs } },
                    "log.and.value");
            }
            else {
                return add<ir::Phi>(
                    std::array<ir::PhiMapping, 2>{
                        ir::PhiMapping{ startBlock, intConstant(1, 1) },
                        ir::PhiMapping{ rhsBlock, rhs } },
                    "log.or.value");
            }
        }();
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
        return add<ir::CompareInst>(getValue(expr.lhs()),
                                    getValue(expr.rhs()),
                                    mapCompareMode(builtinType),
                                    mapCompareOp(expr.operation()),
                                    "cmp.result");
    }

    case Comma:
        return getValue(expr.lhs()), getValue(expr.rhs());

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
        auto* lhs = getValue(expr.lhs());
        auto* rhs = getValue(expr.rhs());
        if (expr.operation() != Assignment) {
            SC_ASSERT(builtinType == expr.rhs()->type()->base(), "");
            auto* lhsValue =
                add<ir::Load>(lhs, mapType(builtinType), "lhs.value");
            rhs =
                add<ir::ArithmeticInst>(lhsValue,
                                        rhs,
                                        mapArithmeticAssignOp(builtinType,
                                                              expr.operation()),
                                        "expr.result");
        }
        return add<ir::Store>(lhs, rhs);
    }
    case BinaryOperator::_count:
        SC_UNREACHABLE();
    }
}

ir::Value* LoweringContext::getValueImpl(MemberAccess const& expr) {
    if (auto itr = valueMap.find(expr.member()->entity());
        itr != valueMap.end())
    {
        return itr->second;
    }
    if (auto* arrayType =
            dyncast<sema::ArrayType const*>(expr.object()->type()->base()))
    {
        SC_ASSERT(expr.member()->value() == "count", "What else?");
        auto* addr = getAddress(expr.object());
        return getArrayCount(addr);
    }

    if (hasAddress(&expr)) {
        return add<ir::Load>(getAddressImpl(expr),
                             mapType(expr.type()),
                             "member.access");
    }
    auto* base       = getValue(expr.object());
    auto* accessedId = cast<Identifier const*>(expr.member());
    auto* var        = cast<sema::Variable const*>(accessedId->entity());
    return add<ir::ExtractValue>(base,
                                 std::array{ var->index() },
                                 "member.access");
}

ir::Value* LoweringContext::getValueImpl(ReferenceExpression const& expr) {
    auto* referred = expr.referred();
    if (referred->type()->isReference()) {
        return getValue(referred);
    }
    return getAddress(referred);
}

ir::Value* LoweringContext::getValueImpl(UniqueExpression const& expr) {
    SC_DEBUGFAIL();
}

ir::Value* LoweringContext::getValueImpl(Conditional const& condExpr) {
    auto* cond      = getValue(condExpr.condition());
    auto* thenBlock = newBlock("cond.then");
    auto* elseBlock = newBlock("cond.else");
    auto* endBlock  = newBlock("cond.end");
    add<ir::Branch>(cond, thenBlock, elseBlock);

    /// Generate then block.
    add(thenBlock);
    auto* thenVal = getValue(condExpr.thenExpr());
    thenBlock     = currentBlock; /// Nested `?:` operands etc. may have changed
                                  /// `currentBlock`
    add<ir::Goto>(endBlock);

    /// Generate else block.
    add(elseBlock);
    auto* elseVal = getValue(condExpr.elseExpr());
    elseBlock     = currentBlock;
    add<ir::Goto>(endBlock);

    /// Generate end block.
    add(endBlock);
    return add<ir::Phi>(std::array<ir::PhiMapping,
                                   2>{ ir::PhiMapping{ thenBlock, thenVal },
                                       ir::PhiMapping{ elseBlock, elseVal } },
                        "cond.result");
}

ir::Value* LoweringContext::getValueImpl(FunctionCall const& expr) {
    return genCall(&expr);
}

ir::Value* LoweringContext::getValueImpl(Subscript const& expr) {
    auto* arrayType =
        cast<sema::ArrayType const*>(expr.object()->type()->base());
    auto* elemType = mapType(arrayType->elementType());
    auto* arrayRef = getAddress(expr.object());
    auto* arrayData =
        arrayType->isDynamic() ? getArrayAddr(arrayRef) : arrayRef;
    auto* index = getValue(expr.arguments().front());
    return add<ir::GetElementPointer>(elemType,
                                      arrayData,
                                      index,
                                      std::initializer_list<size_t>{},
                                      "elem.ptr");
}

ir::Value* LoweringContext::getValueImpl(Conversion const& conv) {
    auto* expr          = conv.expression();
    auto* refConvResult = [&]() -> ir::Value* {
        switch (conv.conversion()->refConversion()) {
        case sema::RefConversion::None:
            return getValue(expr);

        case sema::RefConversion::Dereference: {
            auto* address = getValue(expr);
            return add<ir::Load>(address,
                                 mapType(expr->type()->base()),
                                 utl::strcat(address->name(), ".deref"));
        }
        case sema::RefConversion::TakeAddress:
            return getAddress(expr);
        }
    }();

    switch (conv.conversion()->objectConversion()) {
        using enum sema::ObjectTypeConversion;
    case None:
        return refConvResult;

    case Array_FixedToDynamic: {
        auto* arrayType = cast<sema::ArrayType const*>(expr->type()->base());
        SC_ASSERT(!arrayType->isDynamic(), "Invalid conversion");
        return makeArrayRef(refConvResult, arrayType->count());
    }
    case Reinterpret_ArrayRef_ToByte:
        [[fallthrough]];
    case Reinterpret_ArrayRef_FromByte: {
        SC_ASSERT(expr->type()->isReference(), "");
        SC_ASSERT(conv.type()->isReference(), "");
        auto* fromType = cast<sema::ArrayType const*>(expr->type()->base());
        auto* toType   = cast<sema::ArrayType const*>(conv.type()->base());
        if (toType->isDynamic()) {
            if (fromType->isDynamic()) {
                auto* data  = getArrayAddr(refConvResult);
                auto* count = getArrayCount(refConvResult);
                if (conv.conversion()->objectConversion() ==
                    Reinterpret_ArrayRef_ToByte)
                {
                    count =
                        add<ir::ArithmeticInst>(count,
                                                intConstant(8, 64),
                                                ir::ArithmeticOperation::Mul,
                                                "reinterpret.count");
                }
                else {
                    count =
                        add<ir::ArithmeticInst>(count,
                                                intConstant(8, 64),
                                                ir::ArithmeticOperation::SDiv,
                                                "reinterpret.count");
                }
                return makeArrayRef(data, count);
            }
            size_t count = fromType->count();
            if (conv.conversion()->objectConversion() ==
                Reinterpret_ArrayRef_ToByte)
            {
                count *= 8;
            }
            else {
                count /= 8;
            }
            return makeArrayRef(refConvResult, count);
        }
        SC_ASSERT(!fromType->isDynamic(), "Invalid conversion");
        return refConvResult;
    }

    case Reinterpret_Value:
        return add<ir::ConversionInst>(refConvResult,
                                       mapType(conv.type()),
                                       ir::Conversion::Bitcast,
                                       "reinterpret");

    case Int_Trunc:
        return add<ir::ConversionInst>(refConvResult,
                                       mapType(conv.type()),
                                       ir::Conversion::Trunc,
                                       "trunc");

    case Unsigned_Widen:
        return add<ir::ConversionInst>(refConvResult,
                                       mapType(conv.type()),
                                       ir::Conversion::Zext,
                                       "zext");

    case Signed_Widen:
        return add<ir::ConversionInst>(refConvResult,
                                       mapType(conv.type()),
                                       ir::Conversion::Sext,
                                       "sext");

    case Float_Trunc:
        return add<ir::ConversionInst>(refConvResult,
                                       mapType(conv.type()),
                                       ir::Conversion::Ftrunc,
                                       "ftrunc");

    case Float_Widen:
        return add<ir::ConversionInst>(refConvResult,
                                       mapType(conv.type()),
                                       ir::Conversion::Fext,
                                       "fext");

    case SignedToFloat:
        return add<ir::ConversionInst>(refConvResult,
                                       mapType(conv.type()),
                                       ir::Conversion::StoF,
                                       "stof");

    case UnsignedToFloat:
        return add<ir::ConversionInst>(refConvResult,
                                       mapType(conv.type()),
                                       ir::Conversion::UtoF,
                                       "utof");

    case FloatToSigned:
        return add<ir::ConversionInst>(refConvResult,
                                       mapType(conv.type()),
                                       ir::Conversion::FtoS,
                                       "ftos");

    case FloatToUnsigned:
        return add<ir::ConversionInst>(refConvResult,
                                       mapType(conv.type()),
                                       ir::Conversion::FtoU,
                                       "ftou");
    }
}

ir::Value* LoweringContext::getValueImpl(ListExpression const& list) {
    SC_DEBUGFAIL();
}

/// MARK: getAddress() Implementation

ir::Value* LoweringContext::getAddress(Expression const* expr) {
    return visit(*expr,
                 [this](auto const& expr) { return getAddressImpl(expr); });
}

ir::Value* LoweringContext::getAddressImpl(Literal const& lit) {
    SC_UNREACHABLE();
}

ir::Value* LoweringContext::getAddressImpl(Identifier const& id) {
    auto* address = variableAddressMap[id.entity()];
    SC_ASSERT(address, "Undeclared identifier");
    SC_ASSERT(id.type()->isImplicitRef() || id.isLValue(), "Just to be safe");
    return address;
}

ir::Value* LoweringContext::getAddressImpl(MemberAccess const& expr) {
    auto* base       = getAddress(expr.object());
    auto* accessedId = cast<Identifier const*>(expr.member());
    auto* var        = cast<sema::Variable const*>(accessedId->entity());
    auto* type       = mapType(expr.object()->type()->base());
    auto* address    = add<ir::GetElementPointer>(type,
                                               base,
                                               intConstant(0, 64),
                                               std::array{ var->index() },
                                               "mem.ptr");

    auto* arrayType = dyncast<sema::ArrayType const*>(expr.type()->base());
    if (arrayType && !arrayType->isDynamic() && !expr.type()->isReference()) {
        return makeArrayRef(address, intConstant(arrayType->count(), 64));
    }

    return address;
}

ir::Value* LoweringContext::getAddressImpl(FunctionCall const& expr) {
    SC_ASSERT(expr.type()->isReference(),
              "Can't be l-value so we expect a reference");
    return genCall(&expr);
}

ir::Value* LoweringContext::getAddressImpl(Subscript const& expr) {
    /// Because the subscript espression does not have an address. It is of type
    /// `'X` and thus a call to `getValue()` resolves to the address of the
    /// element
    SC_UNREACHABLE();
}

ir::Value* LoweringContext::getAddressImpl(ReferenceExpression const& expr) {
    return getAddress(expr.referred());
}

ir::Value* LoweringContext::getAddressImpl(Conversion const& conv) {
    auto* expr = conv.expression();
    switch (conv.conversion()->refConversion()) {
    case sema::RefConversion::None:
        return getAddress(expr);
    case sema::RefConversion::Dereference:
        SC_ASSERT(
            !conv.type()->isReference() && expr->type()->isImplicitRef(),
            "--Only of a dereferencing conversion can we take the address--");
        return getValue(expr);
    case sema::RefConversion::TakeAddress:
        SC_UNREACHABLE();
    }
}

static bool evalConstant(Expression const* expr, utl::vector<u8>& dest) {
    auto* val = dyncast_or_null<sema::IntValue const*>(expr->constantValue());
    if (!val) {
        return false;
    }
    auto value            = val->value();
    size_t const elemSize = expr->type()->base()->size();
    auto* data            = reinterpret_cast<u8 const*>(value.limbs().data());
    for (auto* end = data + elemSize; data < end; ++data) {
        dest.push_back(*data);
    }
    return true;
}

bool LoweringContext::genStaticListData(ListExpression const& list,
                                        ir::Alloca* dest) {
    auto* type     = cast<sema::ArrayType const*>(list.type()->base());
    auto* elemType = type->elementType();
    utl::small_vector<u8> data;
    data.reserve(type->size());
    for (auto* expr: list.elements()) {
        SC_ASSERT(elemType == expr->type()->base(), "Invalid type");
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
    add<ir::Call>(memcpy,
                  std::array<ir::Value*, 3>{
                      dest,
                      source,
                      intConstant(list.elements().size() * elemType->size(),
                                  64) },
                  std::string{});
    return true;
}

void LoweringContext::genListDataFallback(ListExpression const& list,
                                          ir::Alloca* dest) {
    auto* arrayType = cast<sema::ArrayType const*>(list.type()->base());
    auto* elemType  = mapType(arrayType->elementType());
    for (auto [index, elem]: list.elements() | ranges::views::enumerate) {
        auto* gep = add<ir::GetElementPointer>(elemType,
                                               dest,
                                               intConstant(index, 32),
                                               std::initializer_list<size_t>{},
                                               "elem.ptr");
        add<ir::Store>(gep, getValue(elem));
    }
}

ir::Value* LoweringContext::getAddressImpl(ListExpression const& list) {
    auto* arrayType = cast<sema::ArrayType const*>(list.type()->base());
    auto* array     = new ir::Alloca(ctx, mapType(arrayType), "list");
    allocas.push_back(array);
    valueMap.insert({ arrayType->countVariable(),
                      intConstant(list.children().size(), 64) });
    if (genStaticListData(list, array)) {
        return array;
    }
    genListDataFallback(list, array);
    return array;
}
