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
    QualType(std::nullptr_t = nullptr): _type(nullptr), _mut{}, _bindMode{} {}

    /// Construct a `QualType` with base type \p type and mutability qualifier
    /// \p mut
    QualType(ObjectType const* type, Mutability mut = Mutability::Mutable,
             PointerBindMode bindMode = PointerBindMode::Static):
        _type(type), _mut(mut), _bindMode(bindMode) {}

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

    /// \Returns `true` if `mutability() == Mutable`
    bool isDyn() const { return bindMode() == PointerBindMode::Dynamic; }

    /// \Returns `true` if `mutability() == Const`
    bool isStatic() const { return bindMode() == PointerBindMode::Static; }

    /// \Returns the binding qualifier
    PointerBindMode bindMode() const { return _bindMode; }

    ///
    QualType to(Mutability mut) const {
        return QualType(get(), mut, bindMode());
    }

    ///
    QualType to(PointerBindMode bindMode) const {
        return QualType(get(), mutability(), bindMode);
    }

    ///
    QualType to(ObjectType const* type) const {
        return QualType(type, mutability(), bindMode());
    }

    ///
    QualType toMut() const { return to(Mutability::Mutable); }

    ///
    QualType toConst() const { return to(Mutability::Const); }

    ///
    QualType toStatic() const { return to(PointerBindMode::Static); }

    ///
    QualType toDyn() const { return to(PointerBindMode::Dynamic); }

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
    size_t hashValue() const {
        return utl::hash_combine(_type, _mut, _bindMode);
    }

private:
    ObjectType const* _type;
    Mutability _mut;
    PointerBindMode _bindMode;
};

} // namespace scatha::sema

template <>
struct std::hash<scatha::sema::QualType> {
    size_t operator()(scatha::sema::QualType type) const {
        return type.hashValue();
    }
};

#endif // SCATHA_SEMA_QUALTYPE_H_
