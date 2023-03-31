#ifndef SCATHA_IR_CFG_CONSTANTS_H_
#define SCATHA_IR_CFG_CONSTANTS_H_

#include "Common/APFloat.h"
#include "Common/APInt.h"

#include "Basic/Basic.h"
#include "IR/CFG/Constant.h"
#include "IR/Common.h"

namespace scatha::ir {

/// Represents a global integral constant value.
class SCATHA_API IntegralConstant: public Constant {
public:
    explicit IntegralConstant(Context& context, APInt value, size_t bitWidth);

    /// \returns The value of this constant.
    APInt const& value() const { return _value; }

private:
    APInt _value;
};

/// Represents a global floating point constant value.
class SCATHA_API FloatingPointConstant: public Constant {
public:
    explicit FloatingPointConstant(Context& context,
                                   APFloat value,
                                   size_t bitWidth);

    /// \returns The value of this constant.
    APFloat const& value() const { return _value; }

private:
    APFloat _value;
};

/// Represents an `undef` value.
class SCATHA_API UndefValue: public Constant {
public:
    explicit UndefValue(Type const* type):
        Constant(NodeType::UndefValue, type) {}
};

} // namespace scatha::ir

#endif // SCATHA_IR_CFG_CONSTANTS_H_
