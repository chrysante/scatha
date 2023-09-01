#ifndef SCATHA_SEMA_QUALTYPE_H_
#define SCATHA_SEMA_QUALTYPE_H_

#include <string>

#include <utl/hash.hpp>

#include <scatha/Sema/Fwd.h>

namespace scatha::sema {

/// Represents a type possibly qualified by mutability qualifiers
/// A `QualType` is a thin wrapper around a pointer to an object type, that also
/// stores a mutability qualifier. This class behaves similar to a smart
/// pointer, but it not not owning
class SCATHA_API QualType {
public:
    /// Construct a const-qualified `QualType`
    static QualType Const(ObjectType const* type) {
        return QualType(type, Mutability::Const);
    }

    /// Construct a mut-qualified `QualType`
    static QualType Mut(ObjectType const* type) {
        return QualType(type, Mutability::Mutable);
    }

    /// Construct an empty `QualType`
    QualType(std::nullptr_t = nullptr): _type(nullptr), _mut{} {}

    /// Construct a `QualType` with base type \p type and mutability qualifier
    /// \p mut
    QualType(ObjectType const* type, Mutability mut = Mutability::Mutable):
        _type(type), _mut(mut) {}

    /// \Returns the unqualified type
    ObjectType const* get() const { return _type; }

    /// \Returns `get()`
    ObjectType const* operator->() const { return get(); }

    /// \Returns `*get()`
    ObjectType const& operator*() const { return *get(); }

    /// \Returns `get() != nullptr`
    explicit operator bool() const { return get() != nullptr; }

    /// \Returns `true` if `mutability() == Mutable`
    bool isMutable() const { return mutability() == Mutability::Mutable; }

    /// \Returns `true` if `mutability() == Const`
    bool isConst() const { return mutability() == Mutability::Const; }

    /// \Returns the mutability qualifier
    Mutability mutability() const { return _mut; }

    ///
    QualType toMutability(Mutability mut) const { return QualType(get(), mut); }

    ///
    QualType toMut() const { return toMutability(Mutability::Mutable); }

    ///
    QualType toConst() const { return toMutability(Mutability::Const); }

    /// Name of this type
    std::string name() const;

    /// Test for equality
    bool operator==(QualType const&) const = default;

    ///
    size_t hashValue() const { return utl::hash_combine(_type, _mut); }

private:
    ObjectType const* _type;
    Mutability _mut;
};

} // namespace scatha::sema

template <>
struct std::hash<scatha::sema::QualType> {
    size_t operator()(scatha::sema::QualType type) const {
        return type.hashValue();
    }
};

#endif // SCATHA_SEMA_QUALTYPE_H_
