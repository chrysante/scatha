#ifndef SCATHA_SEMA_CONSTANTEXPRESSIONS_H_
#define SCATHA_SEMA_CONSTANTEXPRESSIONS_H_

#include "AST/Fwd.h"
#include "Common/APFloat.h"
#include "Common/APInt.h"
#include "Common/UniquePtr.h"
#include "Sema/Fwd.h"

namespace scatha::sema {

/// Represents a constant value
class Value {
public:
    /// Runtime type of this node
    ConstantKind kind() const { return _kind; }

    /// Type of this value
    ObjectType const* type() const { return _type; }

protected:
    explicit Value(ConstantKind kind, ObjectType const* type):
        _kind(kind), _type(type) {}

public:
    ConstantKind _kind;
    ObjectType const* _type;
};

inline ConstantKind dyncast_get_type(Value const& value) {
    return value.kind();
}

/// Represents an integral constant value
class IntValue: public Value {
public:
    explicit IntValue(APInt value, IntType const* type);

    IntType const* type() const;

    /// The value of this constant expression
    APInt const& value() const { return val; }

private:
    APInt val;
};

/// Represents a floating point constant value
class FloatValue: public Value {
public:
    explicit FloatValue(APFloat value, FloatType const* type);

    FloatType const* type() const;

    /// The value of this constant expression
    APFloat const& value() const { return val; }

private:
    APFloat val;
};

UniquePtr<Value> evalUnary(ast::UnaryPrefixOperator op, Value const* operand);

UniquePtr<Value> evalBinary(ast::BinaryOperator op,
                            Value const* lhs,
                            Value const* rhs);

} // namespace scatha::sema

#endif // SCATHA_SEMA_CONSTANTEXPRESSIONS_H_
