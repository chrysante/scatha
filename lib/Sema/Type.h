// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_SEMA_TYPE_H_
#define SCATHA_SEMA_TYPE_H_

#include <span>

#include <utl/vector.hpp>

#include <scatha/Common/Base.h>

#include <scatha/Sema/FunctionSignature.h>
#include <scatha/Sema/Scope.h>
#include <scatha/Sema/Variable.h>

namespace scatha::sema {

size_t constexpr invalidSize = static_cast<size_t>(-1);

/// Abstract class representing a type
class Type: public Scope {
public:
    /// TypeID of this type
    TypeID symbolID() const { return TypeID(Entity::symbolID()); }

    /// Size of this type
    size_t size() const { return _size; }

    /// Align of this type
    size_t align() const { return _align; }

    bool isComplete() const;

    void setSize(size_t value) { _size = value; }

    void setAlign(size_t value) { _align = value; }

protected:
    explicit Type(EntityType entityType,
                  ScopeKind scopeKind,
                  SymbolID typeID,
                  std::string name,
                  Scope* parent,
                  size_t size,
                  size_t align):
        Scope(entityType, scopeKind, std::move(name), typeID, parent),
        _size(size),
        _align(align) {}

private:
    size_t _size;
    size_t _align;
};

/// Concrete class representing the type of an object
class ObjectType: public Type {
public:
    explicit ObjectType(std::string name,
                        SymbolID typeID,
                        Scope* parentScope,
                        size_t size    = invalidSize,
                        size_t align   = invalidSize,
                        bool isBuiltin = false):
        Type(EntityType::ObjectType,
             ScopeKind::Object,
             typeID,
             std::move(name),
             parentScope,
             size,
             align),
        _isBuiltin(isBuiltin) {}

    bool isBuiltin() const { return _isBuiltin; }

    /// The member variables of this type in the order of declaration.
    std::span<SymbolID const> memberVariables() const { return _memberVars; }

    void setIsBuiltin(bool value) { _isBuiltin = value; }

    void addMemberVariable(SymbolID symbolID) {
        _memberVars.push_back(symbolID);
    }

public:
    bool _isBuiltin;
    utl::small_vector<SymbolID> _memberVars;
};

/// Concrete class representing the type of a reference
class ReferenceType: public Type {
public:
    explicit ReferenceType(SymbolID typeID, ObjectType const* referred):
        Type(EntityType::ReferenceType,
             ScopeKind::Invalid,
             typeID,
             std::string{},
             nullptr,
             8,
             8),
        ref(referred) {}

    /// The type referred to
    ObjectType const* referred() const { return ref; }

public:
    ObjectType const* ref;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_TYPE_H_
