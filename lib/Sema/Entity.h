#ifndef SCATHA_SEMA_ENTITY_H_
#define SCATHA_SEMA_ENTITY_H_

#include <concepts>
#include <string>
#include <string_view>
#include <utility>

#include <range/v3/view.hpp>
#include <utl/hashmap.hpp>
#include <utl/vector.hpp>

#include <scatha/Common/Base.h>
#include <scatha/Sema/Fwd.h>

namespace scatha::sema {

/// # Base

/// Base class for all semantic entities in the language.
class SCATHA_API Entity {
public:
    /// The name of this entity
    std::string_view name() const { return _names[0]; }

    /// List of alternate names that refer to this entity
    std::span<std::string const> alternateNames() const { return _names; }

    /// Mangled name of this entity
    std::string const& mangledName() const;

    /// `true` if this entity is unnamed
    bool isAnonymous() const { return name().empty(); }

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

    /// Add \p name as an alternate name for this entity
    void addAlternateName(std::string name);

protected:
    explicit Entity(EntityType entityType, std::string name, Scope* parent):
        _entityType(entityType), _parent(parent), _names({ std::move(name) }) {}

private:
    EntityType _entityType;
    Scope* _parent = nullptr;
    utl::small_vector<std::string, 1> _names;
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
        return dyncast_or_null<E const*>(findEntity(name));
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
    friend class Entity;

    /// Add \p entity as a child of this scope. This function is called by
    /// `SymbolTable`, thus it is a friend
    void add(Entity* entity);

    /// Callback for `Entity::addAlternateName()` to register the new name in
    /// our symbol table Does nothing if \p child is not a child of this scope
    void addAlternateChildName(Entity* child, std::string name);

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

/// Represents the signature, aka parameter types and return type of a function
class SCATHA_API FunctionSignature {
public:
    FunctionSignature() = default;
    explicit FunctionSignature(utl::vector<QualType const*> argumentTypes,
                               QualType const* returnType):
        _argumentTypes(std::move(argumentTypes)), _returnType(returnType) {}

    Type const* type() const { SC_DEBUGFAIL(); }

    /// Argument types
    std::span<QualType const* const> argumentTypes() const {
        return _argumentTypes;
    }

    /// Argument type at index \p index
    QualType const* argumentType(size_t index) const {
        return _argumentTypes[index];
    }

    /// Number of arguments
    size_t argumentCount() const { return _argumentTypes.size(); }

    /// Return type
    QualType const* returnType() const { return _returnType; }

private:
    utl::small_vector<QualType const*> _argumentTypes;
    QualType const* _returnType;
};

/// \Returns `true` iff \p a and \p b have the same argument types
bool argumentsEqual(FunctionSignature const& a, FunctionSignature const& b);

/// Represents a builtin or user defined function
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

    /// Set the signature of this function
    void setSignature(FunctionSignature sig) { _sig = std::move(sig); }

    /// \Returns The signature of this function.
    FunctionSignature const& signature() const { return _sig; }

    /// Return type
    QualType const* returnType() const { return _sig.returnType(); }

    /// Argument types
    std::span<QualType const* const> argumentTypes() const {
        return _sig.argumentTypes();
    }

    /// Argument type at index \p index
    QualType const* argumentType(size_t index) const {
        return _sig.argumentType(index);
    }

    /// Number of arguments
    size_t argumentCount() const { return _sig.argumentCount(); }

    /// Kind of this function
    FunctionKind kind() const { return _kind; }

    /// \Returns `kind() == FunctionKind::Native`
    bool isNative() const { return kind() == FunctionKind::Native; }

    /// \Returns `isExtern() && slot() == svm::BuiltinFunctionSlot`
    bool isBuiltin() const;

    /// \Returns `kind() == FunctionKind::External`
    bool isExtern() const { return kind() == FunctionKind::External; }

    /// \Returns `true` if this is a member function
    bool isMember() const { return _isMember; }

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

    void setKind(FunctionKind kind) { _kind = kind; }

    void setIsMember(bool value = true) { _isMember = value; }

    void setAccessSpecifier(AccessSpecifier spec) { accessSpec = spec; }

    /// Set attribute \p attr to `true`.
    void setAttribute(FunctionAttribute attr) { attrs |= attr; }

    /// Set attribute \p attr to `false`.
    void removeAttribute(FunctionAttribute attr) { attrs &= ~attr; }

private:
    friend class SymbolTable;
    FunctionSignature _sig;
    OverloadSet* _overloadSet = nullptr;
    FunctionAttribute attrs;
    AccessSpecifier accessSpec = AccessSpecifier::Private;
    FunctionKind _kind         = FunctionKind::Native;
    bool _isMember : 1         = false;
    u16 _slot                  = 0;
    u32 _index                 = 0;
};

/// # Types

size_t constexpr InvalidSize = ~size_t(0);

/// Abstract class representing a type
class Type: public Scope {
public:
    /// Size of this type
    size_t size() const;

    /// Align of this type
    size_t align() const;

    bool isComplete() const;

protected:
    explicit Type(EntityType entityType,
                  ScopeKind scopeKind,
                  std::string name,
                  Scope* parent):
        Scope(entityType, scopeKind, std::move(name), parent) {}
};

/// Abstract class representing the type of an object
class ObjectType: public Type {
public:
    explicit ObjectType(EntityType entityType,
                        ScopeKind scopeKind,
                        std::string name,
                        Scope* parent,
                        size_t size,
                        size_t align):
        Type(entityType, scopeKind, std::move(name), parent),
        _size(size),
        _align(align) {}

    void setSize(size_t value) { _size = value; }

    void setAlign(size_t value) { _align = value; }

private:
    friend class Type;
    size_t sizeImpl() const { return _size; }
    size_t alignImpl() const { return _align; }

    size_t _size;
    size_t _align;
};

/// Concrete class representing a builtin type
class BuiltinType: public ObjectType {
protected:
    explicit BuiltinType(EntityType entityType,
                         std::string name,
                         Scope* parentScope,
                         size_t size,
                         size_t align):
        ObjectType(entityType,
                   ScopeKind::Object,
                   std::move(name),
                   parentScope,
                   size,
                   align) {}
};

/// Concrete class representing type `void`
class VoidType: public BuiltinType {
public:
    explicit VoidType(Scope* parentScope);
};

/// Concrete class representing type `void`
class BoolType: public BuiltinType {
public:
    explicit BoolType(Scope* parentScope);
};

/// Concrete class representing type `void`
class ByteType: public BuiltinType {
public:
    explicit ByteType(Scope* parentScope);
};

/// Abstract class representing an arithmetic type
class ArithmeticType: public BuiltinType {
public:
    /// Number of bits in this type
    size_t bitwidth() const { return 8 * size(); }

protected:
    explicit ArithmeticType(EntityType entityType,
                            std::string name,
                            size_t bitwidth,
                            Scope* parentScope):
        BuiltinType(entityType,
                    std::move(name),
                    parentScope,
                    bitwidth / 8,
                    bitwidth / 8) {}
};

/// Concrete class representing an integral type
class IntType: public ArithmeticType {
public:
    enum Signedness { Signed, Unsigned };

    explicit IntType(size_t bitwidth,
                     Signedness signedness,
                     Scope* parentScope);

    /// `Signed` or `Unsigned`
    Signedness signedness() const { return _signed; }

    bool isSigned() const { return signedness() == Signed; }

    bool isUnsigned() const { return signedness() == Unsigned; }

private:
    Signedness _signed;
};

/// Concrete class representing a floating point type
class FloatType: public ArithmeticType {
public:
    explicit FloatType(size_t bitwidth, Scope* parentScope);
};

/// Concrete class representing the type of a structure
class StructureType: public ObjectType {
public:
    explicit StructureType(std::string name,
                           Scope* parentScope,
                           size_t size  = InvalidSize,
                           size_t align = InvalidSize):
        ObjectType(EntityType::StructureType,
                   ScopeKind::Object,
                   std::move(name),
                   parentScope,
                   size,
                   align) {}

    /// The member variables of this type in the order of declaration.
    std::span<Variable* const> memberVariables() { return _memberVars; }

    /// \overload
    std::span<Variable const* const> memberVariables() const {
        return _memberVars;
    }

    void addMemberVariable(Variable* var) { _memberVars.push_back(var); }

public:
    utl::small_vector<Variable*> _memberVars;
};

/// Concrete class representing the type of an array
class ArrayType: public ObjectType {
public:
    static constexpr size_t DynamicCount = ~size_t(0);

    explicit ArrayType(ObjectType const* elementType, size_t count):
        ObjectType(EntityType::ArrayType,
                   ScopeKind::Object,
                   makeName(elementType, count),
                   const_cast<Scope*>(elementType->parent()),
                   count != DynamicCount ? count * elementType->size() :
                                           InvalidSize,
                   elementType->align()),
        elemType(elementType),
        _count(count) {}

    /// Type of the elements in this array
    ObjectType const* elementType() const { return elemType; }

    /// Number of elements in this array
    size_t count() const { return _count; }

    /// Shorthand for `count() == DynamicCount`
    bool isDynamic() const { return count() == DynamicCount; }

    /// Member variable that stores the count
    Variable const* countVariable() const { return countVar; }

    void setCountVariable(Variable* var) { countVar = var; }

private:
    static std::string makeName(ObjectType const* elemType, size_t size);

    ObjectType const* elemType;
    Variable* countVar = nullptr;
    size_t _count;
};

/// Represents a type possibly qualified by reference or mutable qualifiers
class SCATHA_API QualType: public Type {
public:
    explicit QualType(ObjectType* base, Reference ref, Mutability mut);

    /// The base object type that is qualified by this `QualType`
    /// I.e. if this is `&mut int`, then `base()` is `int`
    ObjectType const* base() const { return _base; }

    /// The reference qualifier of this type
    Reference reference() const { return ref; }

    /// \Returns `true` iff this is an explicit reference to const type
    bool isExplicitConstRef() const {
        return reference() == Reference::ConstExplicit;
    }

    /// \Returns `true` iff this is an explicit reference to mutable type
    bool isExplicitMutRef() const {
        return reference() == Reference::MutExplicit;
    }

    /// \Returns `true` iff this is an explicit reference type
    bool isExplicitRef() const {
        return isExplicitConstRef() || isExplicitMutRef();
    }

    /// \Returns `true` iff this is an implicit reference to const type
    bool isImplicitConstRef() const {
        return reference() == Reference::ConstImplicit;
    }

    /// \Returns `true` iff this is an implicit reference to mutable type
    bool isImplicitMutRef() const {
        return reference() == Reference::MutImplicit;
    }

    /// \Returns `true` iff this is an implicit reference type
    bool isImplicitRef() const {
        return isImplicitConstRef() || isImplicitMutRef();
    }

    bool isMutRef() const { return isImplicitMutRef() || isExplicitMutRef(); }

    bool isConstRef() const {
        return isImplicitConstRef() || isExplicitConstRef();
    }

    /// \Returns `true` iff this is any (implicit or explicit) reference type
    bool isReference() const { return isImplicitRef() || isExplicitRef(); }

    /// The mutability qualifier of this type
    Mutability mutability() const { return mut; }

    /// \Returns `true` iff `mutability() == Mutable`
    bool isMutable() const { return mutability() == Mutability::Mutable; }

    /// \Returns `true` iff `mutability() == Const`
    bool isConst() const { return mutability() == Mutability::Const; }

private:
    static std::string makeName(ObjectType* base, Reference ref);

    friend class Type;
    size_t sizeImpl() const;
    size_t alignImpl() const;

    ObjectType* _base;
    Reference ref;
    Mutability mut;
};

/// # OverloadSet

class SCATHA_API OverloadSet:
    public Entity,
    private utl::small_vector<Function*, 8> {
    using VecBase = utl::small_vector<Function*, 8>;

public:
    /// Construct an empty overload set.
    explicit OverloadSet(std::string name, Scope* parentScope):
        Entity(EntityType::OverloadSet, std::move(name), parentScope) {}

    /// Add a function to this overload set.
    /// \returns Pair of \p function and `true` if \p function is a legal
    /// overload. Pair of pointer to existing function that prevents \p function
    /// from being a legal overload and `false` otherwise.
    std::pair<Function const*, bool> add(Function* function);

    /// Inherit interface from `utl::vector`
    using VecBase::begin;
    using VecBase::empty;
    using VecBase::end;
    using VecBase::size;
    using VecBase::operator[];
    using VecBase::back;
    using VecBase::front;
};

/// # Generic

/// Represents a generic, that is a generic class or generic function that can
/// be instantiated on type arguments
class SCATHA_API Generic: public Entity {
public:
    explicit Generic(std::string name, size_t numParams, Scope* parentScope):
        Entity(EntityType::Generic, std::move(name), parentScope) {}

    size_t numParameters() const { return numParams; }

private:
    size_t numParams;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_ENTITY_H_
