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
    QualType(): _type(nullptr), _mut{} {}

    /// Construct an empty `QualType`
    QualType(std::nullptr_t): QualType() {}

    /// Construct a `QualType` with base type \p type
    /// Mutability defaults to mutable
    QualType(ObjectType const* type): QualType(type, Mutability::Mutable) {}

    /// Construct a `QualType` with base type \p type and mutability qualifier
    /// \p mut
    QualType(ObjectType const* type, Mutability mut): _type(type), _mut(mut) {}

    /// \Returns the unqualified type
    ObjectType const* get() const { return _type; }

    /// \Returns `get()`
    ObjectType const* operator->() const { return get(); }

    /// \Returns `*get()`
    ObjectType const& operator*() const { return *get(); }

    /// \Returns `get() != nullptr`
    explicit operator bool() const { return get() != nullptr; }

    /// \Returns `true` if `mutability() == Mutable`
    bool isMut() const { return mutability() == Mutability::Mutable; }

    /// \Returns `true` if `mutability() == Const`
    bool isConst() const { return mutability() == Mutability::Const; }

    /// \Returns the mutability qualifier
    Mutability mutability() const { return _mut; }

    ///
    QualType to(Mutability mut) const { return QualType(get(), mut); }

    ///
    QualType toMut() const { return to(Mutability::Mutable); }

    ///
    QualType toConst() const { return to(Mutability::Const); }

    /// Name of this type with possible top level mutability qualifier
    std::string qualName() const;

    /// Test for equality
    bool operator==(QualType const&) const = default;

    /// We explicitly disable comparisons between raw type pointers and
    /// `QualType`, because they would trigger an implicit conversion of the raw
    /// pointer to `QualType` and then the comparison fails if the mutability
    /// does not match. If comparison of mutability is desired, the qual type
    /// will have to be explicitly constructed, otherwise the raw type pointers
    /// can be compared with `.get()`
    bool operator==(ObjectType const*) const = delete;

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
