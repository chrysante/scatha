// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_SEMA_FUNCTIONSIGNATURE_H_
#define SCATHA_SEMA_FUNCTIONSIGNATURE_H_

#include <span>

#include <utl/vector.hpp>

#include <scatha/Sema/Entity.h>
#include <scatha/Sema/Scope.h>
#include <scatha/Sema/SymbolID.h>

namespace scatha::sema {

class SCATHA_API FunctionSignature {
public:
    FunctionSignature() = default;
    explicit FunctionSignature(utl::vector<TypeID> argumentTypes,
                               TypeID returnType):
        _argumentTypeIDs(std::move(argumentTypes)),
        _returnTypeID(returnType),
        _argHash(hashArguments(argumentTypeIDs())),
        _typeHash(computeTypeHash(returnType, _argHash)) {}

    TypeID typeID() const { return TypeID(_typeHash); }

    /// TypeIDs of the argument types
    std::span<TypeID const> argumentTypeIDs() const { return _argumentTypeIDs; }

    TypeID argumentTypeID(size_t index) const {
        return _argumentTypeIDs[index];
    }

    size_t argumentCount() const { return _argumentTypeIDs.size(); }

    /// TypeID of the return type
    TypeID returnTypeID() const { return _returnTypeID; }

    /// Hash value is computed from the TypeIDs of the arguments
    u64 argumentHash() const { return _argHash; }

    /// Compute hash value from argument types
    static u64 hashArguments(std::span<TypeID const>);

private:
    static u64 computeTypeHash(TypeID returnTypeID, u64 argumentHash);

private:
    utl::small_vector<TypeID> _argumentTypeIDs;
    TypeID _returnTypeID;
    u64 _argHash, _typeHash;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_FUNCTIONSIGNATURE_H_
