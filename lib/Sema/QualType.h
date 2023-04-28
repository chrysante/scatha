#ifndef SCATHA_SEMA_QUALTYPE_H_
#define SCATHA_SEMA_QUALTYPE_H_

#include <utl/common.hpp>

#include <scatha/Sema/Type.h>

namespace scatha::sema {

enum class TypeQualifiers {
    None              = 0,
    Mutable           = 1 << 0,
    ImplicitReference = 1 << 1,
    ExplicitReference = 1 << 2,
    Unique            = 1 << 3,
    Array             = 1 << 4,
};

UTL_ENUM_OPERATORS(TypeQualifiers);

class SCATHA_API QualType: public Type {
public:
    static constexpr size_t DynamicArraySize = ~uint32_t(0);

    explicit QualType(SymbolID typeID,
                      ObjectType* base,
                      TypeQualifiers qualifiers,
                      size_t arraySize = 0):
        Type(EntityType::QualType,
             ScopeKind::Invalid,
             typeID,
             makeName(base, qualifiers, arraySize),
             base->parent(),
             base->size(),
             base->align()),
        _base(base),
        _quals(qualifiers),
        _arraySize(utl::narrow_cast<uint32_t>(arraySize)) {
        if (isReference()) {
            setSize(8);
            setAlign(8);
        }
    }

    /// The base object type that is qualified by this `QualType`
    /// I.e. if this is `&mut int`, then `base()` is `int`
    ObjectType const* base() const { return _base; }

    TypeQualifiers qualifiers() const { return _quals; }

    /// The number of elements in this array. Only applicable is this is an
    /// array type
    size_t arraySize() const { return _arraySize; }

    bool has(TypeQualifiers qual) const { return test(qualifiers() & qual); }

    bool isReference() const {
        return has(TypeQualifiers::ImplicitReference |
                   TypeQualifiers::ExplicitReference);
    }

    /// \Return `true` iff this type is an array or an array reference
    bool isArray() const { return has(TypeQualifiers::Array); }

    /// \Return `true` iff this type is an array reference
    bool isArrayReference() const { return isArray() && isReference(); }

    /// \Return `true` iff this type is a unique reference
    bool isUnique() const { return has(TypeQualifiers::Unique); }

private:
    static std::string makeName(ObjectType* base,
                                TypeQualifiers qualifiers,
                                size_t arraySize);

    ObjectType* _base;
    TypeQualifiers _quals;
    uint32_t _arraySize = 0;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_QUALTYPE_H_
