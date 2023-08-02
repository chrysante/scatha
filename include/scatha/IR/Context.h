// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_IR_CONTEXT_H_
#define SCATHA_IR_CONTEXT_H_

#include <memory>
#include <span>
#include <string>

#include <scatha/Common/APFloat.h>
#include <scatha/Common/APInt.h>
#include <scatha/Common/UniquePtr.h>
#include <scatha/IR/Fwd.h>

namespace scatha::ir {

class SCATHA_API Context {
public:
    /// Construct an empty `Context` object
    Context();
    Context(Context const& rhs)            = delete;
    Context& operator=(Context const& rhs) = delete;
    Context(Context&&) noexcept;
    Context& operator=(Context&&) noexcept;

    ~Context();

    /// \returns The `void` type
    VoidType const* voidType();

    /// \returns The `ptr` type
    PointerType const* pointerType();

    /// \returns The `iN` type where `N` is \p bitwidth
    IntegralType const* integralType(size_t bitwidth);

    /// \returns The `fN` type where `N` is \p bitwidth
    /// \param bitwidth must be either 32 or 64
    FloatType const* floatType(size_t bitwidth);

    /// \returns The structure type with members \p members
    StructureType const* anonymousStructure(
        std::span<Type const* const> members);

    /// \returns The array type of \p elementType with \p count elements
    ArrayType const* arrayType(Type const* elementType, size_t count);

    /// \returns The global integral constant with value \p value
    IntegralConstant* integralConstant(APInt value);

    /// \returns The global integral constant of \p bitwidth bits with value \p
    /// value
    IntegralConstant* integralConstant(u64 value, size_t bitwidth);

    /// \returns The global floating point constant of \p bitwidth bits with
    /// value \p value
    FloatingPointConstant* floatConstant(APFloat value, size_t bitwidth);

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

    /// \returns The `undef` constant of type \p type
    UndefValue* undef(Type const* type);

    /// \returns An opaque value of type void.
    Value* voidValue();

    /// \Returns `true` iff associativity of floating point arithmetic
    /// operations may be assumed
    bool associativeFloatArithmetic() const { return true; /*  For now */ }

    /// \Returns `true` iff \p operation is commutative
    bool isCommutative(ArithmeticOperation operation) const;

    /// \Returns `true` iff \p operation is associative
    bool isAssociative(ArithmeticOperation operation) const;

private:
    struct Impl;

    std::unique_ptr<Impl> impl;
};

} // namespace scatha::ir

#endif // SCATHA_IR_CONTEXT_H_