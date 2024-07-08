#ifndef SCATHA_IR_CONTEXT_H_
#define SCATHA_IR_CONTEXT_H_

#include <concepts>
#include <memory>
#include <span>
#include <string>

#include <scatha/Common/APMathFwd.h>
#include <scatha/Common/UniquePtr.h>
#include <scatha/IR/Fwd.h>

namespace scatha::ir {

/// Manages types and constants
class SCATHA_API Context {
public:
    /// Construct an empty `Context` object
    Context();
    Context(Context const& rhs) = delete;
    Context& operator=(Context const& rhs) = delete;
    Context(Context&&) noexcept;
    Context& operator=(Context&&) noexcept;

    ~Context();

    /// \returns The `void` type
    VoidType const* voidType();

    /// \returns The `ptr` type
    PointerType const* ptrType();

    /// \returns The `iN` type where `N` is \p bitwidth
    IntegralType const* intType(size_t bitwidth);

    /// \returns The `i1` type
    IntegralType const* boolType();

    /// \returns The `fN` type where `N` is \p bitwidth
    /// \param bitwidth must be either 32 or 64
    FloatType const* floatType(size_t bitwidth);

    /// \returns The `fN` type where `N` is either 32 or 64 depending on \p
    /// precision
    FloatType const* floatType(APFloatPrec precision);

    /// \returns The structure type with members \p members
    StructType const* anonymousStruct(std::span<Type const* const> members);

    /// \returns The structure type with members \p members
    StructType const* anonymousStruct(
        std::initializer_list<Type const*> members) {
        return anonymousStruct(std::span<Type const* const>(members));
    }

    /// \returns The array type of \p elementType with \p count elements
    ArrayType const* arrayType(Type const* elementType, size_t count);

    /// \returns The array type of `i8` with \p count elements
    ArrayType const* byteArrayType(size_t count);

    /// \returns The global integral constant with value \p value
    IntegralConstant* intConstant(APInt value);

    /// \returns The global integral constant of \p bitwidth bits with value
    /// \p value
    IntegralConstant* intConstant(u64 value, size_t bitwidth);

    /// \overload
    template <std::integral T>
    IntegralConstant* intConstant(T value, size_t bitwidth) {
        if constexpr (std::is_signed_v<T>) {
            return intConstant((uint64_t)(int64_t)value, bitwidth);
        }
        else {
            return intConstant((uint64_t)value, bitwidth);
        }
    }

    /// \returns The global bool constant  value \p  value
    IntegralConstant* boolConstant(bool value);

    /// \returns The global floating point constant with value \p value
    FloatingPointConstant* floatConstant(APFloat value);

    /// \returns The global floating point constant of \p bitwidth bits with
    /// value \p value
    FloatingPointConstant* floatConstant(double value, size_t bitwidth);

    /// \returns The global integral or floating point constant of type \p type
    /// with value \p value
    Constant* arithmeticConstant(int64_t value, Type const* type);

    /// \returns The integral constant with value \p value
    Constant* arithmeticConstant(APInt value);

    /// \returns The floating point constant with value \p value
    Constant* arithmeticConstant(APFloat value);

    /// \Returns the record object of type \p type with constant elements
    /// \p elems
    RecordConstant* recordConstant(std::span<ir::Constant* const> elems,
                                   RecordType const* type);

    /// \Returns the struct object of type \p type with constant elements
    /// \p elems
    StructConstant* structConstant(std::span<ir::Constant* const> elems,
                                   StructType const* type);

    ///
    StructConstant* anonymousStructConstant(
        std::span<ir::Constant* const> elems);

    /// \Returns the array of type \p type with constant elements \p elems
    ArrayConstant* arrayConstant(std::span<ir::Constant* const> elems,
                                 ArrayType const* type);

    /// \Returns the array of type \p type with constant elements \p text
    ArrayConstant* stringLiteral(std::string_view text);

    /// \Returns the null pointer constant
    NullPointerConstant* nullpointer();

    /// \Returns the zero constant of type \p type
    Constant* nullConstant(Type const* type);

    /// \returns The `undef` constant of type \p type
    UndefValue* undef(Type const* type);

    /// \returns An opaque value of type void.
    Value* voidValue();

    /// \Returns `true` if associativity of floating point arithmetic
    /// operations may be assumed
    bool associativeFloatArithmetic() const { return true; /*  For now */ }

    /// \Returns `true` if \p operation is commutative
    bool isCommutative(ArithmeticOperation operation) const;

    /// \Returns `true` if \p operation is associative
    bool isAssociative(ArithmeticOperation operation) const;

private:
    struct Impl;

    std::unique_ptr<Impl> impl;
};

} // namespace scatha::ir

#endif // SCATHA_IR_CONTEXT_H_
