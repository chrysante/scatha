#ifndef SCATHA_SEMA_ENTITY_H_
#define SCATHA_SEMA_ENTITY_H_

#include <concepts>
#include <string>
#include <string_view>
#include <utility>

#include <range/v3/view.hpp>
#include <utl/hashmap.hpp>

#include <scatha/Common/Base.h>
#include <scatha/Sema/FunctionSignature.h>
#include <scatha/Sema/Fwd.h>

namespace scatha::sema {

/// # Base

/// Base class for all semantic entities in the language.
class SCATHA_API Entity {
public:
    /// The name of this entity
    std::string_view name() const { return _name; }

    /// Mangled name of this entity
    std::string const& mangledName() const;

    /// `true` if this entity is unnamed
    bool isAnonymous() const { return _name.empty(); }

    /// The parent scope of this entity
    Scope* parent() { return _parent; }

    /// \overload
    Scope const* parent() const { return _parent; }

    /// The runtime type of this entity class
    EntityType entityType() const { return _entityType; }

    /// Category this entity belongs to
    EntityCategory category() const { return categorize(entityType()); }

    /// `true` if this entity represents a value
    bool isValue() const { return category() == EntityCategory::Value; }

    /// `true` if this entity represents a type
    bool isType() const { return category() == EntityCategory::Type; }

protected:
    explicit Entity(EntityType entityType, std::string name, Scope* parent):
        _entityType(entityType), _parent(parent), _name(name) {}

private:
    EntityType _entityType;
    Scope* _parent = nullptr;
    std::string _name;
    mutable std::string _mangledName;
};

EntityType dyncast_get_type(std::derived_from<Entity> auto const& entity) {
    return entity.entityType();
}

namespace internal {
class ScopePrinter;
}

/// # Variable

/// Represents a variable
class SCATHA_API Variable: public Entity {
public:
    explicit Variable(std::string name,
                      Scope* parentScope,
                      QualType const* type = nullptr);

    /// Set the type of this variable.
    void setType(QualType const* type) { _type = type; }

    /// Set the offset of this variable.
    void setOffset(size_t offset) { _offset = offset; }

    /// Set the index of this variable.
    void setIndex(size_t index) { _index = index; }

    /// Type of this variable.
    QualType const* type() const { return _type; }

    /// Offset into the struct this variable is a member of. If this is not a
    /// member variable then offset() == 0.
    [[deprecated("Use index() instead")]] size_t offset() const {
        return _offset;
    }

    /// If this variable is a member of a struct, this is the position of this
    /// variable in the struct.
    size_t index() const { return _index; }

    /// Wether this variable is local to a function or potentially globally
    /// visible.
    bool isLocal() const;

private:
    QualType const* _type;
    size_t _offset = 0;
    size_t _index  = 0;
};

/// # Scopes

/// Represents a scope
class SCATHA_API Scope: public Entity {
public:
    /// The kind of this scope
    ScopeKind kind() const { return _kind; }

    /// Find entity by name within this scope
    Entity* findEntity(std::string_view name) {
        return const_cast<Entity*>(std::as_const(*this).findEntity(name));
    }

    /// \overload
    Entity const* findEntity(std::string_view name) const;

    /// Find entity by name and `dyncast` to type `E`
    template <std::derived_from<Entity> E>
    E* findEntity(std::string_view name) {
        return const_cast<E*>(std::as_const(*this).findEntity<E>(name));
    }

    /// \overload
    template <std::derived_from<Entity> E>
    E const* findEntity(std::string_view name) const {
        Entity const* result = findEntity(name);
        return result ? dyncast<E const*>(result) : nullptr;
    }

    /// \Returns `true` iff \p scope is a child scope of this
    bool isChildScope(Scope const* scope) const {
        return _children.contains(scope);
    }

    /// \Returns A View over the children of this scope
    auto children() const {
        return _children | ranges::views::transform(
                               [](auto* scope) -> auto* { return scope; });
    }

    /// \Returns A View over the entities in this scope
    auto entities() const {
        return _entities | ranges::views::transform(
                               [](auto& p) -> auto& { return p.second; });
    }

protected:
    explicit Scope(EntityType entityType,
                   ScopeKind,
                   std::string name,
                   Scope* parent);

private:
    friend class internal::ScopePrinter;
    friend class SymbolTable;
    void add(Entity* entity);

private:
    /// Scopes don't own their childscopes. These objects are owned by the
    /// symbol table.
    utl::hashset<Scope*> _children;
    utl::hashmap<std::string, Entity*> _entities;
    ScopeKind _kind;
};

/// Represents an anonymous scope
class AnonymousScope: public Scope {
public:
    explicit AnonymousScope(ScopeKind scopeKind, Scope* parent);
};

/// Represents the global scope
class GlobalScope: public Scope {
public:
    explicit GlobalScope();
};

/// # Function

class SCATHA_API Function: public Scope {
public:
    explicit Function(std::string name,
                      OverloadSet* overloadSet,
                      Scope* parentScope,
                      FunctionAttribute attrs):
        Scope(EntityType::Function,
              ScopeKind::Function,
              std::move(name),
              parentScope),
        _overloadSet(overloadSet),
        attrs(attrs) {}

    /// \Returns The type ID of this function.
    Type const* type() const { return signature().type(); }

    /// \Returns The overload set of this function.
    OverloadSet* overloadSet() { return _overloadSet; }

    /// \overload
    OverloadSet const* overloadSet() const { return _overloadSet; }

    /// \Returns The signature of this function.
    FunctionSignature const& signature() const { return _sig; }

    /// \Returns `true` if this is a member function
    bool isMember() const { return _isMember; }

    /// \Returns `true` iff this function is an external function.
    bool isExtern() const { return _isExtern; }

    /// \returns Slot of extern function table.
    ///
    /// Only applicable if this function is extern.
    size_t slot() const { return _slot; }

    /// \returns Index into slot of extern function table.
    ///
    /// Only applicable if this function is extern.
    size_t index() const { return _index; }

    /// \returns Bitfield of function attributes
    FunctionAttribute attributes() const { return attrs; }

    AccessSpecifier accessSpecifier() const { return accessSpec; }

    void setIsMember(bool value = true) { _isMember = value; }

    void setAccessSpecifier(AccessSpecifier spec) { accessSpec = spec; }

    /// Set attribute \p attr to `true`.
    void setAttribute(FunctionAttribute attr) { attrs |= attr; }

    /// Set attribute \p attr to `false`.
    void removeAttribute(FunctionAttribute attr) { attrs &= ~attr; }

private:
    friend class SymbolTable; /// To set `_sig` and `_isExtern`
    FunctionSignature _sig;
    OverloadSet* _overloadSet = nullptr;
    FunctionAttribute attrs;
    AccessSpecifier accessSpec = AccessSpecifier::Private;
    bool _isMember : 1         = false;
    bool _isExtern : 1         = false;
    u16 _slot                  = 0;
    u32 _index                 = 0;
};

/// # Types

size_t constexpr invalidSize = static_cast<size_t>(-1);

/// Abstract class representing a type
class Type: public Scope {
public:
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
                  std::string name,
                  Scope* parent,
                  size_t size,
                  size_t align):
        Scope(entityType, scopeKind, std::move(name), parent),
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
                        Scope* parentScope,
                        size_t size    = invalidSize,
                        size_t align   = invalidSize,
                        bool isBuiltin = false):
        Type(EntityType::ObjectType,
             ScopeKind::Object,
             std::move(name),
             parentScope,
             size,
             align),
        _isBuiltin(isBuiltin) {}

    bool isBuiltin() const { return _isBuiltin; }

    /// The member variables of this type in the order of declaration.
    std::span<Variable* const> memberVariables() { return _memberVars; }

    /// \overload
    std::span<Variable const* const> memberVariables() const {
        return _memberVars;
    }

    void setIsBuiltin(bool value) { _isBuiltin = value; }

    void addMemberVariable(Variable* var) { _memberVars.push_back(var); }

public:
    bool _isBuiltin;
    utl::small_vector<Variable*> _memberVars;
};

class SCATHA_API QualType: public Type {
public:
    static constexpr size_t DynamicArraySize = ~uint32_t(0);

    explicit QualType(ObjectType* base,
                      TypeQualifiers qualifiers,
                      size_t arraySize = 0):
        Type(EntityType::QualType,
             ScopeKind::Invalid,
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

    bool isExplicitReference() const {
        return has(TypeQualifiers::ExplicitReference);
    }

    bool isImplicitReference() const {
        return has(TypeQualifiers::ImplicitReference);
    }

    bool isReference() const {
        return isImplicitReference() || isExplicitReference();
    }

    /// \Return `true` iff this type is an array or an array reference
    bool isArray() const { return has(TypeQualifiers::Array); }

    /// \Return `true` iff this type is an array reference
    bool isArrayReference() const { return isArray() && isReference(); }

    /// \Return `true` iff this type is mutable
    bool isMutable() const { return has(TypeQualifiers::Mutable); }

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

/// # OverloadSet

class SCATHA_API OverloadSet: public Entity {
public:
    /// Construct an empty overload set.
    explicit OverloadSet(std::string name, Scope* parentScope):
        Entity(EntityType::OverloadSet, std::move(name), parentScope) {}

    /// Resolve best matching function from this overload set for \p
    /// argumentTypes Returns `nullptr` if no matching function exists in the
    /// overload set.
    Function* find(std::span<QualType const* const> argumentTypes) {
        return const_cast<Function*>(std::as_const(*this).find(argumentTypes));
    }

    /// \overload
    Function const* find(std::span<QualType const* const> argumentTypes) const;

    /// \brief Add a function to this overload set.
    /// \returns Pair of \p function and `true` if \p function is a legal
    /// overload. Pair of pointer to existing function that prevents \p function
    /// from being a legal overload and `false` otherwise.
    std::pair<Function const*, bool> add(Function* function);

    /// Begin iterator to set of `Function`'s
    auto begin() const { return functions.begin(); }
    /// End iterator to set of `Function`'s
    auto end() const { return functions.end(); }

private:
    utl::small_vector<Function*, 8> functions;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_ENTITY_H_
