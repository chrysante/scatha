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

UniquePtr<Value> Value::clone() const {
    return visit(*this, [](auto& child) -> UniquePtr<Value> {
        return child.doClone();
    });
}

UniquePtr<IntValue> IntValue::doClone() const {
    return allocate<IntValue>(value(), isSigned());
}

UniquePtr<FloatValue> FloatValue::doClone() const {
    return allocate<FloatValue>(value());
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
                return allocate<IntValue>(*result, value.isSigned());
            }
            return nullptr;
        },
        [&](FloatValue const& value) -> UniquePtr<Value> {
            if (auto result = doEvalUnary(op, value.value())) {
                return allocate<FloatValue>(*result);
            }
            return nullptr;
        },
    }); // clang-format on
}

static UniquePtr<Value> doEvalBinary(ast::BinaryOperator op,
                                     bool isSigned,
                                     APInt lhs,
                                     APInt rhs) {
    using enum ast::BinaryOperator;
    int const cmpResult = isSigned ? scmp(lhs, rhs) : ucmp(lhs, rhs);
    switch (op) {
    case Multiplication:
        return allocate<IntValue>(mul(lhs, rhs), isSigned);
    case Division:
        return allocate<IntValue>(isSigned ? sdiv(lhs, rhs) : udiv(lhs, rhs),
                                  isSigned);
    case Remainder:
        return allocate<IntValue>(isSigned ? srem(lhs, rhs) : urem(lhs, rhs),
                                  isSigned);
    case Addition:
        return allocate<IntValue>(add(lhs, rhs), isSigned);
    case Subtraction:
        return allocate<IntValue>(sub(lhs, rhs), isSigned);
    case LeftShift:
        return allocate<IntValue>(lshl(lhs, rhs.to<int>()), false);
    case RightShift:
        return allocate<IntValue>(lshr(lhs, rhs.to<int>()), false);
    case Less:
        return allocate<IntValue>(APInt(cmpResult < 0, 1), false);
    case LessEq:
        return allocate<IntValue>(APInt(cmpResult <= 0, 1), false);
    case Greater:
        return allocate<IntValue>(APInt(cmpResult > 0, 1), false);
    case GreaterEq:
        return allocate<IntValue>(APInt(cmpResult >= 0, 1), false);
    case Equals:
        return allocate<IntValue>(APInt(cmpResult == 0, 1), false);
    case NotEquals:
        return allocate<IntValue>(APInt(cmpResult != 0, 1), false);
    case LogicalAnd:
        SC_ASSERT(!isSigned && lhs.bitwidth() == 1, "Must be bool");
        [[fallthrough]];
    case BitwiseAnd:
        return allocate<IntValue>(btwand(lhs, rhs), isSigned);
    case BitwiseXOr:
        return allocate<IntValue>(btwxor(lhs, rhs), isSigned);
    case LogicalOr:
        SC_ASSERT(!isSigned && lhs.bitwidth() == 1, "Must be bool");
        [[fallthrough]];
    case BitwiseOr:
        return allocate<IntValue>(btwor(lhs, rhs), isSigned);

    default:
        return nullptr;
    }
}

static UniquePtr<Value> doEvalBinary(ast::BinaryOperator op,
                                     APFloat lhs,
                                     APFloat rhs) {
    using enum ast::BinaryOperator;
    int const cmpResult = cmp(lhs, rhs);
    switch (op) {
    case Multiplication:
        return allocate<FloatValue>(mul(lhs, rhs));
    case Division:
        return allocate<FloatValue>(div(lhs, rhs));
    case Addition:
        return allocate<FloatValue>(add(lhs, rhs));
    case Subtraction:
        return allocate<FloatValue>(sub(lhs, rhs));
    case Less:
        return allocate<IntValue>(APInt(cmpResult < 0, 1), false);
    case LessEq:
        return allocate<IntValue>(APInt(cmpResult <= 0, 1), false);
    case Greater:
        return allocate<IntValue>(APInt(cmpResult > 0, 1), false);
    case GreaterEq:
        return allocate<IntValue>(APInt(cmpResult >= 0, 1), false);
    case Equals:
        return allocate<IntValue>(APInt(cmpResult == 0, 1), false);
    case NotEquals:
        return allocate<IntValue>(APInt(cmpResult != 0, 1), false);
    default:
        return nullptr;
    }
}

UniquePtr<Value> sema::evalBinary(ast::BinaryOperator op,
                                  Value const* lhs,
                                  Value const* rhs) {
    if (op == ast::BinaryOperator::Comma) {
        return clone(rhs);
    }
    SC_ASSERT(lhs, "Must not be null");
    SC_ASSERT(rhs, "");
    SC_ASSERT(lhs->kind() == rhs->kind(), "");
    // clang-format off
    return visit(*lhs, utl::overload{
        [&](IntValue const& lhs) -> UniquePtr<Value> {
            return doEvalBinary(op,
                                lhs.isSigned(),
                                lhs.value(),
                                cast<IntValue const&>(*rhs).value());
        },
        [&](FloatValue const& lhs) -> UniquePtr<Value> {
            return doEvalBinary(op,
                                lhs.value(),
                                cast<FloatValue const&>(*rhs).value());
        },
    }); // clang-format on
}

static std::pair<size_t, bool> widthAndSign(ObjectType const* type) {
    using R = std::pair<size_t, bool>;
    // clang-format off
    return visit(*type, utl::overload{
        [](BoolType const& type) -> R {
            return { 1, false };
        },
        [](ByteType const& type) -> R {
            return { 8, false };
        },
        [](IntType const& type) -> R {
            return { type.bitwidth(), type.isSigned() };
        },
        [](FloatType const& type) -> R {
            return { type.bitwidth(), true };
        },
        [](ObjectType const& type) -> R { SC_UNREACHABLE(); },
    }); // clang-format on
}

static UniquePtr<Value> doEvalConversion(sema::Conversion const* conv,
                                         IntValue const* operand) {
    auto [targetWidth, targetSigned] = widthAndSign(conv->targetType()->base());
    auto value                       = operand->value();
    using enum ObjectTypeConversion;
    switch (conv->objectConversion()) {
    case None:
        return allocate<IntValue>(zext(value, targetWidth), targetSigned);

    case Reinterpret_Value:
        return nullptr;

    case Int_Trunc:
        return allocate<IntValue>(zext(value, targetWidth), targetSigned);

    case Signed_Widen:
        return allocate<IntValue>(sext(value, targetWidth), targetSigned);

    case Unsigned_Widen:
        return allocate<IntValue>(zext(value, targetWidth), targetSigned);

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

UniquePtr<Value> sema::evalConditional(Value const* condition,
                                       Value const* thenValue,
                                       Value const* elseValue) {
    auto* intCond = dyncast<IntValue const*>(condition);
    SC_ASSERT(intCond && intCond->isBool(), "Must be bool");
    return ucmp(intCond->value(), 0) != 0 ? clone(thenValue) : clone(elseValue);
}
