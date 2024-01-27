#ifndef SCATHA_SEMA_THINEXPR_H_
#define SCATHA_SEMA_THINEXPR_H_

#include "AST/Fwd.h"
#include "Sema/Fwd.h"
#include "Sema/QualType.h"

namespace scatha::sema {

/// TODO: Find better name
/// Represents certain information about an expression like type and value
/// category. This class is used to pass type and value category of expressions
/// to analysis functions
class ThinExpr {
public:
    /// Construct implicitly from an expression
    ThinExpr(ast::Expression const* expr);

    /// Construct from fields
    ThinExpr(QualType type, ValueCategory valueCat):
        _type(type), _valueCat(valueCat) {}

    /// \Returns the type of this expression
    QualType type() const { return _type; }

    /// \Returns the value category of this expression
    ValueCategory valueCategory() const { return _valueCat; }

    /// \Returns `valueCategory() == ValueCategory::LValue`
    bool isLValue() const { return valueCategory() == ValueCategory::LValue; }

    /// \Returns `valueCategory() == ValueCategory::RValue`
    bool isRValue() const { return valueCategory() == ValueCategory::RValue; }

private:
    QualType _type;
    ValueCategory _valueCat;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_THINEXPR_H_
