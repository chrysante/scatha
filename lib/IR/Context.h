// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_IR_CONTEXT_H_
#define SCATHA_IR_CONTEXT_H_

#include <string>

#include <utl/hashmap.hpp>
#include <utl/hashset.hpp>
#include <utl/strcat.hpp>
#include <utl/vector.hpp>

#include <scatha/Common/APFloat.h>
#include <scatha/Common/APInt.h>
#include <scatha/Common/UniquePtr.h>
#include <scatha/IR/Common.h>

namespace scatha::ir {

class SCATHA_API Context {
public:
    /// Construct an empty `Context` object
    Context();

    Context(Context&&) noexcept;

    Context& operator=(Context&&) noexcept;

    ~Context();

    /// \returns The `void` type
    VoidType const* voidType() { return _voidType; }

    /// \returns The `ptr` type
    PointerType const* pointerType() { return _ptrType; }

    /// \returns The `iN` type where `N` is \p bitWidth
    IntegralType const* integralType(size_t bitWidth);

    /// \returns The `fN` type where `N` is \p bitWidth
    /// \param bitWidth must be either 32 or 64
    FloatType const* floatType(size_t bitWidth);

    /// \returns The global integral constant with value \p value
    IntegralConstant* integralConstant(APInt value);

    /// \returns The global integral constant of \p bitWidth bits with value \p
    /// value
    IntegralConstant* integralConstant(u64 value, size_t bitWidth);

    /// \returns The global floating point constant of \p bitWidth bits with
    /// value \p value
    FloatingPointConstant* floatConstant(APFloat value, size_t bitWidth);

    /// \returns The global floating point constant of \p bitWidth bits with
    /// value \p value
    FloatingPointConstant* floatConstant(double value, size_t bitWidth);

    /// \returns The global integral or floating point constant of type \p type
    /// with value \p value
    Constant* arithmeticConstant(int64_t value, Type const* type);

    /// \returns The `undef` constant of type \p type
    UndefValue* undef(Type const* type);

    /// \returns An opaque value of type void.
    Value* voidValue();

    /// \Returns `true` iff associativity of floating point arithmetic
    /// operations may be assumed
    bool commutativeFloatArithmetic() const { return true; /*  For now */ }

private:
    /// ## Constants
    /// ** Bitwidth must appear before the value, because comparison of values
    /// of different widths may not be possible. **
    utl::hashmap<std::pair<size_t, APInt>, UniquePtr<IntegralConstant>>
        _integralConstants;
    /// We use `std::map` here because floats are not really hashable.
    utl::hashmap<std::pair<size_t, APFloat>, UniquePtr<FloatingPointConstant>>
        _floatConstants;
    utl::hashmap<Type const*, UniquePtr<UndefValue>> _undefConstants;

    /// ## Types
    utl::vector<UniquePtr<Type>> _types;
    VoidType const* _voidType;
    PointerType const* _ptrType;
    utl::hashmap<uint32_t, IntegralType const*> _intTypes;
    utl::hashmap<uint32_t, FloatType const*> _floatTypes;
};

} // namespace scatha::ir

#endif // SCATHA_IR_CONTEXT_H_
