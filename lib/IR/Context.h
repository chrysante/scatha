// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_IR_CONTEXT_H_
#define SCATHA_IR_CONTEXT_H_

#include <map>
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

    /// \returns the `void` type
    VoidType const* voidType() { return _voidType; }

    /// \returns the `ptr` type
    PointerType const* pointerType() { return _ptrType; }

    /// \returns the `iN` type where `N` is \p bitWidth
    IntegralType const* integralType(size_t bitWidth);

    /// \returns the `fN` type where `N` is \p bitWidth
    /// \param bitWidth must be either 32 or 64
    FloatType const* floatType(size_t bitWidth);

    /// \returns the global integral constant with value \p value
    IntegralConstant* integralConstant(APInt value);

    /// \returns the global integral constant of \p bitWidth bits with value \p
    /// value
    IntegralConstant* integralConstant(u64 value, size_t bitWidth);

    /// \returns the global floating point constant of \p bitWidth bits with
    /// value \p value
    FloatingPointConstant* floatConstant(APFloat value, size_t bitWidth);

    /// \returns the global floating point constant of \p bitWidth bits with
    /// value \p value
    FloatingPointConstant* floatConstant(double value, size_t bitWidth);

    /// \returns the `undef` constant of type \p type
    UndefValue* undef(Type const* type);

    /// \returns an opaque value of type void
    Value* voidValue();

private:
    /// ## Constants
    utl::hashmap<std::pair<APInt, size_t>, IntegralConstant*>
        _integralConstants;
    /// We use `std::map` here because floats are not really hashable.
    std::map<std::pair<APFloat, size_t>, FloatingPointConstant*>
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
