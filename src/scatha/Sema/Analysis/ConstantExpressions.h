#ifndef SCATHA_SEMA_CONSTANTEXPRESSIONS_H_
#define SCATHA_SEMA_CONSTANTEXPRESSIONS_H_

#include "AST/Fwd.h"
#include "Common/APFloat.h"
#include "Common/APInt.h"
#include "Common/UniquePtr.h"
#include "Sema/Fwd.h"

namespace scatha::sema {

/// Represents a constant value
class Value: public csp::base_helper<Value> {
public:
    /// Runtime type of this node
    ConstantKind kind() const { return get_rtti(*this); }

    /// Clone this value
    UniquePtr<Value> clone() const;

protected:
    using base_helper::base_helper;
};

inline UniquePtr<Value> clone(Value const* value) {
    return value ? value->clone() : nullptr;
}

/// Represents an integral constant value
class IntValue: public Value {
public:
    explicit IntValue(APInt value, bool isSigned):
        Value(ConstantKind::IntValue),
        val(std::move(value)),
        _signed(isSigned) {}

    /// The value of this constant expression
    APInt const& value() const { return val; }

    ///
    size_t bitwidth() const { return val.bitwidth(); }

    ///
    bool isSigned() const { return _signed; }

    ///
    bool isBool() const { return value().bitwidth() == 1 && !isSigned(); }

private:
    friend class Value;
    UniquePtr<IntValue> doClone() const;

    APInt val;
    bool _signed;
};

/// Represents a floating point constant value
class FloatValue: public Value {
public:
    explicit FloatValue(APFloat value):
        Value(ConstantKind::FloatValue), val(std::move(value)) {}

    /// The value of this constant expression
    APFloat const& value() const { return val; }

private:
    friend class Value;
    UniquePtr<FloatValue> doClone() const;

    APFloat val;
};

///
class PointerValue: public Value {
public:
    struct NullPointerTag {};

    /// Constructs a null pointer constant value
    PointerValue(NullPointerTag): Value(ConstantKind::PointerValue) {}

    /// \Returns `true` if this value is the null pointer
    bool isNull() const { return true; }

private:
    friend class Value;
    UniquePtr<PointerValue> doClone() const;

    // For now we only support null pointer constants
};

UniquePtr<Value> evalUnary(ast::UnaryOperator op, Value const* operand);

UniquePtr<Value> evalBinary(ast::BinaryOperator op, Value const* lhs,
                            Value const* rhs);

UniquePtr<Value> evalConversion(sema::ObjectTypeConversion conv,
                                Value const* operand);

UniquePtr<Value> evalConditional(Value const* condition, Value const* thenValue,
                                 Value const* elseValue);

} // namespace scatha::sema

#endif // SCATHA_SEMA_CONSTANTEXPRESSIONS_H_
