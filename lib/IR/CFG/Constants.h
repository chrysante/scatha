#ifndef SCATHA_IR_CFG_CONSTANTS_H_
#define SCATHA_IR_CFG_CONSTANTS_H_

#include "Common/APFloat.h"
#include "Common/APInt.h"

#include "Common/Base.h"
#include "IR/CFG/Constant.h"
#include "IR/Fwd.h"

namespace scatha::ir {

/// Represents a global integral constant value.
class SCATHA_API IntegralConstant: public Constant {
public:
    explicit IntegralConstant(Context& context, APInt value, size_t bitwidth);

    /// \returns The value of this constant.
    APInt const& value() const { return _value; }

    /// \Returns The type of this constant as `IntegralType`.
    IntegralType const* type() const;

private:
    APInt _value;
};

/// Represents a global floating point constant value.
class SCATHA_API FloatingPointConstant: public Constant {
public:
    explicit FloatingPointConstant(Context& context,
                                   APFloat value,
                                   size_t bitwidth);

    /// \returns The value of this constant.
    APFloat const& value() const { return _value; }

    /// \Returns The type of this constant as `FloatType`.
    FloatType const* type() const;

private:
    APFloat _value;
};

/// Represents an `undef` value.
class SCATHA_API UndefValue: public Constant {
public:
    explicit UndefValue(Type const* type):
        Constant(NodeType::UndefValue, type) {}
};

/// Represents constant data
class SCATHA_API ConstantData: public Constant {
public:
    ConstantData(ir::Context& ctx,
                 Type const* dataType,
                 utl::vector<uint8_t> data,
                 std::string name);

    /// The constant data
    std::span<uint8_t const> data() const { return _data; }

    /// The type of this value is `ptr`, so this exists to query the type of the
    /// data
    ir::Type const* dataType() const { return _dataType; }

private:
    Type const* _dataType = nullptr;
    utl::vector<uint8_t> _data;
};

} // namespace scatha::ir

#endif // SCATHA_IR_CFG_CONSTANTS_H_
