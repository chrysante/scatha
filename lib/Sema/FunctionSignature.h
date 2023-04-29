// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_SEMA_FUNCTIONSIGNATURE_H_
#define SCATHA_SEMA_FUNCTIONSIGNATURE_H_

#include <span>

#include <utl/vector.hpp>

#include <scatha/Sema/Fwd.h>

namespace scatha::sema {

class SCATHA_API FunctionSignature {
public:
    FunctionSignature() = default;
    explicit FunctionSignature(utl::vector<QualType const*> argumentTypes,
                               QualType const* returnType):
        _argumentTypes(std::move(argumentTypes)), _returnType(returnType) {}

    Type const* type() const { SC_DEBUGFAIL(); }

    /// TypeIDs of the argument types
    std::span<QualType const* const> argumentTypes() const {
        return _argumentTypes;
    }

    QualType const* argumentType(size_t index) const {
        return _argumentTypes[index];
    }

    size_t argumentCount() const { return _argumentTypes.size(); }

    /// TypeID of the return type
    QualType const* returnType() const { return _returnType; }

private:
    utl::small_vector<QualType const*> _argumentTypes;
    QualType const* _returnType;
};

/// \Returns `true` iff \p a and \p b have the same argument types
bool argumentsEqual(FunctionSignature const& a, FunctionSignature const& b);

} // namespace scatha::sema

#endif // SCATHA_SEMA_FUNCTIONSIGNATURE_H_
