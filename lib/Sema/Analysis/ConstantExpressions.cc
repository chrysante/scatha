#include "Sema/Analysis/ConstantExpressions.h"

#include <optional>

#include "Sema/Analysis/Conversion.h"
#include "Sema/Entity.h"

using namespace scatha;
using namespace sema;

void scatha::internal::privateDelete(sema::Value* value) {
    visit(*value, [](auto& value) { delete &value; });
}

void scatha::internal::privateDestroy(sema::Value* type) {
    visit(*type, [](auto& type) { std::destroy_at(&type); });
}

IntValue::IntValue(APInt value, IntType const* type):
    Value(ConstantKind::IntValue, type), val(value) {
    SC_ASSERT(value.bitwidth() == type->bitwidth(), "");
}

IntType const* IntValue::type() const {
    return cast<IntType const*>(Value::type());
}

FloatValue::FloatValue(APFloat value, FloatType const* type):
    Value(ConstantKind::FloatValue, type), val(value) {}

FloatType const* FloatValue::type() const {
    return cast<FloatType const*>(Value::type());
}

static std::optional<APInt> doEvalUnary(ast::UnaryPrefixOperator op,
                                        APInt operand) {
    using enum ast::UnaryPrefixOperator;
    switch (op) {
    case Promotion:
        return operand;
    case Negation:
        return negate(operand);
    case BitwiseNot:
        return btwnot(operand);
    default:
        return std::nullopt;
    }
}

static std::optional<APFloat> doEvalUnary(ast::UnaryPrefixOperator op,
                                          APFloat operand) {
    using enum ast::UnaryPrefixOperator;
    switch (op) {
    case Promotion:
        return operand;
    case Negation:
        return negate(operand);
    default:
        return std::nullopt;
    }
}

UniquePtr<Value> sema::evalUnary(ast::UnaryPrefixOperator op,
                                 Value const* operand) {
    SC_ASSERT(operand, "Must not be null");
    // clang-format off
    return visit(*operand, utl::overload{
        [&](IntValue const& value) -> UniquePtr<Value> {
            if (auto result = doEvalUnary(op, value.value())) {
                return allocate<IntValue>(*result, value.type());
            }
            return nullptr;
        },
        [&](FloatValue const& value) -> UniquePtr<Value> {
            if (auto result = doEvalUnary(op, value.value())) {
                return allocate<FloatValue>(*result, value.type());
            }
            return nullptr;
        },
    }); // clang-format on
}

static std::optional<APInt> doEvalBinary(ast::BinaryOperator op,
                                         bool isSigned,
                                         APInt lhs,
                                         APInt rhs) {
    using enum ast::BinaryOperator;
    switch (op) {
    case Multiplication:
        return mul(lhs, rhs);
    case Division:
        return isSigned ? sdiv(lhs, rhs) : udiv(lhs, rhs);
    case Remainder:
        return isSigned ? srem(lhs, rhs) : urem(lhs, rhs);
    case Addition:
        return add(lhs, rhs);
    case Subtraction:
        return sub(lhs, rhs);
    case LeftShift:
        return lshl(lhs, rhs.to<int>());
    case RightShift:
        return lshr(lhs, rhs.to<int>());
    case Less:
        return std::nullopt;
    case LessEq:
        return std::nullopt;
    case Greater:
        return std::nullopt;
    case GreaterEq:
        return std::nullopt;
    case Equals:
        return std::nullopt;
    case NotEquals:
        return std::nullopt;
    case BitwiseAnd:
        return btwand(lhs, rhs);
    case BitwiseXOr:
        return btwxor(lhs, rhs);
    case BitwiseOr:
        return btwor(lhs, rhs);
    default:
        return std::nullopt;
    }
}

static std::optional<APFloat> doEvalBinary(ast::BinaryOperator op,
                                           APFloat lhs,
                                           APFloat rhs) {
    using enum ast::BinaryOperator;
    switch (op) {
    case Multiplication:
        return mul(lhs, rhs);
    case Division:
        return div(lhs, rhs);
    case Addition:
        return add(lhs, rhs);
    case Subtraction:
        return sub(lhs, rhs);
    default:
        return std::nullopt;
    }
}

UniquePtr<Value> sema::evalBinary(ast::BinaryOperator op,
                                  Value const* lhs,
                                  Value const* rhs) {
    if (op == ast::BinaryOperator::Comma) {
        return visit(*rhs, []<typename T>(T const& rhs) -> UniquePtr<Value> {
            return allocate<T>(rhs.value(), rhs.type());
        });
    }
    SC_ASSERT(lhs, "Must not be null");
    SC_ASSERT(rhs, "");
    SC_ASSERT(lhs->kind() == rhs->kind(), "");
    // clang-format off
    return visit(*lhs, utl::overload{
        [&](IntValue const& lhs) -> UniquePtr<Value> {
            auto result = doEvalBinary(op,
                                       lhs.type()->isSigned(),
                                       lhs.value(),
                                       cast<IntValue const&>(*rhs).value());
            if (result) {
                return allocate<IntValue>(*result, lhs.type());
            }
            return nullptr;
        },
        [&](FloatValue const& lhs) -> UniquePtr<Value> {
            auto result = doEvalBinary(op,
                                       lhs.value(),
                                       cast<FloatValue const&>(*rhs).value());
            if (result) {
                return allocate<FloatValue>(*result, lhs.type());
            }
            return nullptr;
        },
    }); // clang-format on
}

static UniquePtr<Value> doEvalConversion(sema::Conversion const* conv,
                                         IntValue const* operand) {
    SC_ASSERT(conv->originType()->base() == operand->type(), "");
    auto* type       = operand->type();
    auto* targetType = cast<ArithmeticType const*>(conv->targetType()->base());
    auto value       = operand->value();
    using enum ObjectTypeConversion;
    switch (conv->objectConversion()) {
    case None:
        return allocate<IntValue>(zext(value, targetType->bitwidth()),
                                  cast<IntType const*>(targetType));

    case Reinterpret_Value:
        return nullptr;

    case Int_Trunc:
        return allocate<IntValue>(zext(value, targetType->bitwidth()),
                                  cast<IntType const*>(targetType));

    case Signed_Widen:
        return allocate<IntValue>(sext(value, targetType->bitwidth()),
                                  cast<IntType const*>(targetType));

    case Unsigned_Widen:
        return allocate<IntValue>(zext(value, targetType->bitwidth()),
                                  cast<IntType const*>(targetType));

    case Float_Trunc:
        return nullptr;

    case Float_Widen:
        return nullptr;

    case SignedToFloat:
        return nullptr;

    case UnsignedToFloat:
        return nullptr;

    case FloatToSigned:
        return nullptr;

    case FloatToUnsigned:
        return nullptr;

    default:
        return nullptr;
    }
}

static UniquePtr<Value> doEvalConversion(sema::Conversion const* conv,
                                         FloatValue const* operand) {
    return nullptr;
}

UniquePtr<Value> sema::evalConversion(sema::Conversion const* conv,
                                      Value const* operand) {
    return visit(*operand,
                 [&](auto& op) { return doEvalConversion(conv, &op); });
}
