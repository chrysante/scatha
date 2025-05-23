#ifndef SCATHA_IR_CFG_CONSTANTS_H_
#define SCATHA_IR_CFG_CONSTANTS_H_

#include <span>

#include <utl/vector.hpp>

#include <scatha/Common/APFloat.h>
#include <scatha/Common/APInt.h>
#include <scatha/Common/Base.h>
#include <scatha/IR/CFG/Constant.h>
#include <scatha/IR/Fwd.h>

namespace scatha::ir {

/// Represents a global integral constant value.
class SCATHA_API IntegralConstant: public Constant {
public:
    explicit IntegralConstant(Context& context, APInt value);

    /// \returns The value of this constant.
    APInt const& value() const { return _value; }

    /// \Returns The type of this constant as `IntegralType`.
    IntegralType const* type() const;

private:
    friend class Constant;
    void writeValueToImpl(
        void* dest,
        utl::function_view<void(Constant const*, void*)> callback) const;

    APInt _value;
};

/// Represents a global floating point constant value.
class SCATHA_API FloatingPointConstant: public Constant {
public:
    explicit FloatingPointConstant(Context& context, APFloat value);

    /// \returns The value of this constant.
    APFloat const& value() const { return _value; }

    /// \Returns The type of this constant as `FloatType`.
    FloatType const* type() const;

private:
    friend class Constant;
    void writeValueToImpl(
        void* dest,
        utl::function_view<void(Constant const*, void*)> callback) const;

    APFloat _value;
};

/// Represents the value of a null pointer
class SCATHA_API NullPointerConstant: public Constant {
public:
    explicit NullPointerConstant(PointerType const* ptrType);

    /// \Returns The type of this constant as `PointerType`.
    PointerType const* type() const;

private:
    friend class Constant;
    void writeValueToImpl(
        void* dest,
        utl::function_view<void(Constant const*, void*)> callback) const;
};

/// Represents an `undef` value.
class SCATHA_API UndefValue: public Constant {
public:
    explicit UndefValue(Type const* type):
        Constant(NodeType::UndefValue, type) {}

private:
    friend class Constant;
    void writeValueToImpl(
        void*, utl::function_view<void(Constant const*, void*)>) const {}
};

/// Represents a constant record
class SCATHA_API RecordConstant: public Constant {
public:
    /// \Returns The type of this constant as `RecordType`.
    RecordType const* type() const;

    /// \Returns a view over the elements as constants
    auto elements() {
        return operands() | ranges::views::transform(cast<Constant*>);
    }

    /// \overload
    auto elements() const {
        return operands() | ranges::views::transform(cast<Constant const*>);
    }

    /// \Returns the number of members or array elements
    size_t numElements() const { return numOperands(); }

    /// \Returns the member or array element at index \p index
    Constant* elementAt(size_t index) {
        return const_cast<Constant*>(std::as_const(*this).elementAt(index));
    }

    /// \overload
    Constant const* elementAt(size_t index) const {
        return elements()[utl::narrow_cast<ssize_t>(index)];
    }

protected:
    explicit RecordConstant(NodeType nodeType,
                            std::span<ir::Constant* const> elems,
                            RecordType const* type);

private:
    friend class Constant;
    void writeValueToImpl(
        void* dest,
        utl::function_view<void(Constant const*, void*)> callback) const;
};

/// Represents a constant struct
class SCATHA_API StructConstant: public RecordConstant {
public:
    explicit StructConstant(std::span<ir::Constant* const> elems,
                            StructType const* type);

    /// \Returns The type of this constant as struct type
    StructType const* type() const;
};

/// Represents a constant array
class SCATHA_API ArrayConstant: public RecordConstant {
public:
    explicit ArrayConstant(std::span<ir::Constant* const> elems,
                           ArrayType const* type);

    /// \Returns The type of this constant as array type
    ArrayType const* type() const;
};

} // namespace scatha::ir

#endif // SCATHA_IR_CFG_CONSTANTS_H_
