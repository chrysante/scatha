#ifndef SCATHA_SEMA_QUALTYPE_H_
#define SCATHA_SEMA_QUALTYPE_H_

#include <utl/common.hpp>

#include <scatha/Sema/Type.h>

namespace scatha::sema {

enum class TypeQualifiers {
    None              = 0,
    ImplicitReference = 1 << 0,
    ExplicitReference = 1 << 1,
    Mutable           = 1 << 2,
};

UTL_ENUM_OPERATORS(TypeQualifiers);

class SCATHA_API QualType: public Type {
public:
    explicit QualType(SymbolID typeID,
                      ObjectType* base,
                      TypeQualifiers qualifiers):
        Type(EntityType::QualType,
             ScopeKind::Invalid,
             typeID,
             makeName(base, qualifiers),
             base->parent(),
             base->size(),
             base->align()),
        _base(base),
        _quals(qualifiers) {
        if (isReference()) {
            setSize(8);
            setAlign(8);
        }
    }

    ObjectType const* base() const { return _base; }

    TypeQualifiers qualifiers() const { return _quals; }

    bool has(TypeQualifiers qual) const { return test(qualifiers() & qual); }

    bool isReference() const {
        return has(TypeQualifiers::ImplicitReference |
                   TypeQualifiers::ExplicitReference);
    }

private:
    static std::string makeName(ObjectType* base, TypeQualifiers qualifiers);

    ObjectType* _base;
    TypeQualifiers _quals;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_QUALTYPE_H_
