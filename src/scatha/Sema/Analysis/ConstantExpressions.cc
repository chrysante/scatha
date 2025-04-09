#include "Sema/Analysis/ConstantExpressions.h"

#include <optional>

#include "Sema/Analysis/Conversion.h"
#include "Sema/Entity.h"

using namespace scatha;
using namespace sema;

void sema::do_delete(sema::Value& value) {
    visit(value, [](auto& value) { delete &value; });
}

void sema::do_destroy(sema::Value& type) {
    visit(type, [](auto& type) { std::destroy_at(&type); });
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

UniquePtr<PointerValue> PointerValue::doClone() const {
    SC_ASSERT(isNull(), "");
    return allocate<PointerValue>(NullPointerTag{});
}

static std::optional<APInt> doEvalUnary(ast::UnaryOperator op, APInt operand) {
    using enum ast::UnaryOperator;
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

static std::optional<APFloat> doEvalUnary(ast::UnaryOperator op,
                                          APFloat operand) {
    using enum ast::UnaryOperator;
    switch (op) {
    case Promotion:
        return operand;
    case Negation:
        return negate(operand);
    default:
        return std::nullopt;
    }
}

UniquePtr<Value> sema::evalUnary(ast::UnaryOperator op, Value const* operand) {
    if (!operand) {
        return nullptr;
    }
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
        [&](Value const&) -> UniquePtr<Value> { return nullptr; },
    }); // clang-format on
}

static UniquePtr<Value> doEvalCmp(ast::BinaryOperator op, bool isSigned,
                                  APInt lhs, APInt rhs) {
    int const cmpResult = isSigned ? scmp(lhs, rhs) : ucmp(lhs, rhs);
    using enum ast::BinaryOperator;
    switch (op) {
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
        SC_UNREACHABLE();
    }
}

static UniquePtr<Value> doEvalBinary(ast::BinaryOperator op,
                                     IntValue const* lhs, IntValue const* rhs) {
    using enum ast::BinaryOperator;
    switch (op) {
    case Multiplication:
        if (lhs && lhs->value() == 0) {
            return allocate<IntValue>(APInt(0, lhs->value().bitwidth()),
                                      lhs->isSigned());
        }
        if (rhs && rhs->value() == 0) {
            return allocate<IntValue>(APInt(0, rhs->value().bitwidth()),
                                      rhs->isSigned());
        }
        if (!lhs || !rhs) {
            return nullptr;
        }
        SC_EXPECT(lhs->isSigned() == rhs->isSigned());
        return allocate<IntValue>(mul(lhs->value(), rhs->value()),
                                  lhs->isSigned());

    case Division:
        if (!lhs || !rhs) {
            return nullptr;
        }
        SC_EXPECT(lhs->isSigned() == rhs->isSigned());
        return allocate<IntValue>(lhs->isSigned() ?
                                      sdiv(lhs->value(), rhs->value()) :
                                      udiv(lhs->value(), rhs->value()),
                                  lhs->isSigned());

    case Remainder:
        if (!lhs || !rhs) {
            return nullptr;
        }
        SC_EXPECT(lhs->isSigned() == rhs->isSigned());
        return allocate<IntValue>(lhs->isSigned() ?
                                      srem(lhs->value(), rhs->value()) :
                                      urem(lhs->value(), rhs->value()),
                                  lhs->isSigned());

    case Addition:
        if (!lhs || !rhs) {
            return nullptr;
        }
        SC_EXPECT(lhs->isSigned() == rhs->isSigned());
        return allocate<IntValue>(add(lhs->value(), rhs->value()),
                                  lhs->isSigned());

    case Subtraction:
        if (!lhs || !rhs) {
            return nullptr;
        }
        SC_EXPECT(lhs->isSigned() == rhs->isSigned());
        return allocate<IntValue>(sub(lhs->value(), rhs->value()),
                                  lhs->isSigned());

    case LeftShift:
        if (!lhs || !rhs) {
            return nullptr;
        }
        SC_EXPECT(lhs->isSigned() == rhs->isSigned());
        return allocate<IntValue>(lshl(lhs->value(), rhs->value().to<int>()),
                                  lhs->isSigned());
    case RightShift:
        if (!lhs || !rhs) {
            return nullptr;
        }
        SC_EXPECT(lhs->isSigned() == rhs->isSigned());
        return allocate<IntValue>(lshr(lhs->value(), rhs->value().to<int>()),
                                  lhs->isSigned());
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
    case NotEquals:
        if (!lhs || !rhs) {
            return nullptr;
        }
        return doEvalCmp(op, lhs->isSigned(), lhs->value(), rhs->value());

    case LogicalAnd:
        if (lhs && !lhs->value().test(0)) {
            SC_ASSERT(lhs->isBool(), "Must be bool");
            return allocate<IntValue>(APInt::False(), false);
        }
        if (rhs && !rhs->value().test(0)) {
            SC_ASSERT(rhs->isBool(), "Must be bool");
            return allocate<IntValue>(APInt::False(), false);
        }
        if (!lhs || !rhs) {
            return nullptr;
        }
        SC_ASSERT(lhs->isBool() && rhs->isBool(), "Must be bool");
        return allocate<IntValue>(btwand(lhs->value(), rhs->value()), 1);

    case BitwiseAnd:
        if (!lhs || !rhs) {
            return nullptr;
        }
        return allocate<IntValue>(btwand(lhs->value(), rhs->value()),
                                  lhs->isSigned());

    case BitwiseXOr:
        if (!lhs || !rhs) {
            return nullptr;
        }
        return allocate<IntValue>(btwxor(lhs->value(), rhs->value()),
                                  lhs->isSigned());

    case LogicalOr:
        if (lhs && lhs->value().test(0)) {
            SC_ASSERT(lhs->isBool(), "Must be bool");
            return allocate<IntValue>(APInt::True(), false);
        }
        if (rhs && rhs->value().test(0)) {
            SC_ASSERT(rhs->isBool(), "Must be bool");
            return allocate<IntValue>(APInt::True(), false);
        }
        if (!lhs || !rhs) {
            return nullptr;
        }
        SC_ASSERT(lhs->isBool() && rhs->isBool(), "Must be bool");
        return allocate<IntValue>(btwor(lhs->value(), rhs->value()), 1);

    case BitwiseOr:
        if (!lhs || !rhs) {
            return nullptr;
        }
        return allocate<IntValue>(btwor(lhs->value(), rhs->value()),
                                  lhs->isSigned());

    default:
        return nullptr;
    }
}

static UniquePtr<Value> doEvalBinary(ast::BinaryOperator op, APFloat lhs,
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

UniquePtr<Value> sema::evalBinary(ast::BinaryOperator op, Value const* lhs,
                                  Value const* rhs) {
    if (!lhs && !rhs) {
        return nullptr;
    }
    if (op == ast::BinaryOperator::Comma) {
        return clone(rhs);
    }
    ConstantKind kind = lhs ? lhs->kind() : rhs->kind();
    switch (kind) {
    case ConstantKind::Value:
        SC_UNREACHABLE();
    case ConstantKind::IntValue:
        return doEvalBinary(op, cast<IntValue const*>(lhs),
                            cast<IntValue const*>(rhs));
    case ConstantKind::FloatValue:
        if (!lhs || !rhs) {
            return nullptr;
        }
        return doEvalBinary(op, cast<FloatValue const*>(lhs)->value(),
                            cast<FloatValue const*>(rhs)->value());
    case ConstantKind::PointerValue:
        return nullptr;
    }
}

static UniquePtr<Value> convTrunc(IntValue const* operand,
                                  size_t targetBitwidth) {
    return allocate<IntValue>(zext(operand->value(), targetBitwidth),
                              operand->isSigned());
}

static UniquePtr<Value> convWiden(IntValue const* operand,
                                  size_t targetBitwidth) {
    bool isSigned = operand->isSigned();
    APInt value = [&] {
        if (isSigned) {
            return sext(operand->value(), targetBitwidth);
        }
        else {
            return zext(operand->value(), targetBitwidth);
        }
    }();
    return allocate<IntValue>(value, isSigned);
}

static UniquePtr<Value> toFloat(IntValue const* operand,
                                size_t targetBitwidth) {
    if (operand->isSigned()) {
        return allocate<FloatValue>(
            signedValuecast<APFloat>(operand->value(), targetBitwidth));
    }
    else {
        return allocate<FloatValue>(
            valuecast<APFloat>(operand->value(), targetBitwidth));
    }
}

static UniquePtr<Value> doEvalConversion(ObjectTypeConversion conv,
                                         IntValue const* operand) {
    using enum ObjectTypeConversion;
    switch (conv) {
    case IntTruncTo8:
        return convTrunc(operand, 8);
    case IntTruncTo16:
        return convTrunc(operand, 16);
    case IntTruncTo32:
        return convTrunc(operand, 32);
    case SignedWidenTo16:
        return convWiden(operand, 16);
    case SignedWidenTo32:
        return convWiden(operand, 32);
    case SignedWidenTo64:
        return convWiden(operand, 64);
    case UnsignedWidenTo16:
        return convWiden(operand, 16);
    case UnsignedWidenTo32:
        return convWiden(operand, 32);
    case UnsignedWidenTo64:
        return convWiden(operand, 64);
    case SignedToUnsigned:
        return allocate<IntValue>(operand->value(), false);
    case UnsignedToSigned:
        return allocate<IntValue>(operand->value(), true);
    case SignedToFloat32:
        return toFloat(operand, 32);
    case SignedToFloat64:
        return toFloat(operand, 64);
    case UnsignedToFloat32:
        return toFloat(operand, 32);
    case UnsignedToFloat64:
        return toFloat(operand, 64);
    case IntToByte:
        SC_ASSERT(operand->bitwidth() == 8, "");
        return operand->clone();
    case ByteToSigned:
        return allocate<IntValue>(operand->value(), true);
    case ByteToUnsigned:
        return allocate<IntValue>(operand->value(), false);
    default:
        return nullptr;
    }
}

static UniquePtr<Value> toSigned(FloatValue const* operand, size_t bitwidth) {
    return allocate<IntValue>(signedValuecast<APInt>(operand->value(),
                                                     bitwidth),
                              true);
}

static UniquePtr<Value> toUnsigned(FloatValue const* operand, size_t bitwidth) {
    return allocate<IntValue>(valuecast<APInt>(operand->value(), bitwidth),
                              true);
}

static UniquePtr<Value> doEvalConversion(ObjectTypeConversion conv,
                                         FloatValue const* operand) {
    APFloat value = operand->value();
    using enum ObjectTypeConversion;
    switch (conv) {
    case FloatTruncTo32:
        return allocate<FloatValue>(
            APFloat(value.to<float>(), APFloatPrec::Single()));
    case FloatWidenTo64:
        return allocate<FloatValue>(
            APFloat(value.to<double>(), APFloatPrec::Single()));
    case FloatToSigned8:
        return toSigned(operand, 8);
    case FloatToSigned16:
        return toSigned(operand, 16);
    case FloatToSigned32:
        return toSigned(operand, 32);
    case FloatToSigned64:
        return toSigned(operand, 64);
    case FloatToUnsigned8:
        return toUnsigned(operand, 8);
    case FloatToUnsigned16:
        return toUnsigned(operand, 16);
    case FloatToUnsigned32:
        return toUnsigned(operand, 32);
    case FloatToUnsigned64:
        return toUnsigned(operand, 64);
    default:
        return nullptr;
    }
}

static UniquePtr<Value> doEvalConversion(ObjectTypeConversion conv,
                                         PointerValue const* value) {
    using enum ObjectTypeConversion;
    switch (conv) {
    case NullptrToRawPtr:
        [[fallthrough]];
    case NullptrToUniquePtr:
        SC_ASSERT(value->isNull(), "");
        return value->clone();
    case UniqueToRawPtr:
        if (value->isNull()) {
            return value->clone();
        }
        return nullptr;
    default:
        return nullptr;
    }
}

UniquePtr<Value> sema::evalConversion(ObjectTypeConversion conv,
                                      Value const* operand) {
    if (!operand) {
        return nullptr;
    }
    return visit(*operand,
                 [&](auto& op) { return doEvalConversion(conv, &op); });
}

UniquePtr<Value> sema::evalConditional(Value const* condition,
                                       Value const* thenValue,
                                       Value const* elseValue) {
    auto* intCond = dyncast<IntValue const*>(condition);
    if (!intCond) {
        return nullptr;
    }
    return ucmp(intCond->value(), 0) != 0 ? clone(thenValue) : clone(elseValue);
}
