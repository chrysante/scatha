// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_IR_CONTEXT_H_
#define SCATHA_IR_CONTEXT_H_

#include <map> // For now, we shouldn't actually use this.
#include <string>

#include <utl/hashmap.hpp>
#include <utl/hashset.hpp>
#include <utl/strcat.hpp>

#include <scatha/Common/APFloat.h>
#include <scatha/Common/APInt.h>
#include <scatha/Common/UniquePtr.h>
#include <scatha/IR/Common.h>

namespace scatha::ir {

class SCATHA(API) Context {
public:
    Context();

    VoidType const* voidType();

    PointerType const* pointerType();

    IntegralType const* integralType(size_t bitWidth);

    FloatType const* floatType(size_t bitWidth);

    IntegralConstant* integralConstant(APInt value);

    IntegralConstant* integralConstant(u64 value, size_t bitWidth);

    FloatingPointConstant* floatConstant(APFloat value, size_t bitWidth);

    UndefValue* undef(Type const* type);

    /// \Returns an opaque value of type void
    Value* voidValue();

    void addGlobal(Constant* constant);

    Constant* getGlobal(std::string_view name) const;

    std::string uniqueName(Function const* function, std::string name);
    std::string uniqueName(Function const* function, auto const&... args) {
        return uniqueName(function, utl::strcat(args...));
    }

private:
    /// ## Constants
    utl::hashmap<std::pair<APInt, size_t>, IntegralConstant*>
        _integralConstants;
    std::map<std::pair<APFloat, size_t>, FloatingPointConstant*>
        _floatConstants;
    utl::hashmap<Type const*, UndefValue*> _undefConstants;

    /// ## Types
    utl::hashmap<std::string, UniquePtr<Type>> _types;

    utl::hashmap<std::string, Constant*> _globals;

    // For unique names
    utl::hashmap<std::pair<Function const*, std::string>, size_t> varIndices;
};

} // namespace scatha::ir

#endif // SCATHA_IR_CONTEXT_H_
