#ifndef SCATHA_SEMA_ENTITY_H_
#define SCATHA_SEMA_ENTITY_H_

#include <array>
#include <concepts>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include <range/v3/view.hpp>
#include <utl/functional.hpp>
#include <utl/hashmap.hpp>
#include <utl/vector.hpp>

#include <scatha/Common/Base.h>
#include <scatha/Common/Ranges.h>
#include <scatha/Common/UniquePtr.h>
#include <scatha/Sema/Fwd.h>
#include <scatha/Sema/QualType.h>

/// Class hierarchy of `Entity`
///
/// ```
/// Entity
/// ├─ Object
/// │  ├─ Variable
/// │  └─ Temporary
/// ├─ OverloadSet
/// ├─ Generic
/// ├─ Scope
/// │  ├─ GlobalScope
/// │  ├─ AnonymousScope
/// │  ├─ Function
/// │  └─ Type
/// │     ├─ ObjectType
/// │     │  ├─ BuiltinType
/// │     │  │  ├─ VoidType
/// │     │  │  └─ ArithmeticType
/// │     │  │     ├─ BoolType
/// │     │  │     ├─ ByteType
/// │     │  │     ├─ IntType
/// │     │  │     └─ FloatType
/// │     │  ├─ StructureType
/// │     │  ├─ ArrayType
/// │     │  └─ ReferenceType
/// │     └─ FunctionType [??, does not exist]
/// └─ PoisonEntity
/// ```

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
    EntityCategory category() const;

    /// `true` if this entity represents a value
    bool isValue() const { return category() == EntityCategory::Value; }

    /// `true` if this entity represents a type
    bool isType() const { return category() == EntityCategory::Type; }

    /// `true` if this entity is a builtin
    bool isBuiltin() const { return _isBuiltin; }

    /// Add \p name as an alternate name for this entity
    void addAlternateName(std::string name);

    /// Mark or unmark this entity as builtin
    void setBuiltin(bool value = true) { _isBuiltin = value; }

protected:
    explicit Entity(EntityType entityType, std::string name, Scope* parent):
        _entityType(entityType), _parent(parent), _names({ std::move(name) }) {}

private:
    EntityCategory categoryImpl() const {
        return EntityCategory::Indeterminate;
    }

    EntityType _entityType;
    bool _isBuiltin = false;
    Scope* _parent = nullptr;
    utl::small_vector<std::string, 1> _names;
    mutable std::string _mangledName;
};

EntityType dyncast_get_type(std::derived_from<Entity> auto const& entity) {
    return entity.entityType();
}

/// Represents an object
class SCATHA_API Object: public Entity {
public:
    SC_MOVEONLY(Object);

    ~Object();

    /// Set the type of this object.
    void setType(QualType type) { _type = type; }

    /// Type of this object.
    QualType type() const { return _type; }

    /// \Returns Constant value if this variable is `const` and has a
    /// const-evaluable initializer `nullptr` otherwise
    Value const* constantValue() const { return constVal.get(); }

    /// Set the constant value of this variable
    void setConstantValue(UniquePtr<Value> value);

protected:
    explicit Object(EntityType entityType,
                    std::string name,
                    Scope* parentScope,
                    QualType type);

private:
    friend class Entity;
    EntityCategory categoryImpl() const { return EntityCategory::Value; }

    QualType _type;
    UniquePtr<Value> constVal;
};

/// Represents a variable
class SCATHA_API Variable: public Object {
public:
    SC_MOVEONLY(Variable);

    explicit Variable(std::string name,
                      Scope* parentScope,
                      QualType type = nullptr);

    /// Set the offset of this variable.
    void setOffset(size_t offset) { _offset = offset; }

    /// Set the index of this variable.
    void setIndex(size_t index) { _index = index; }

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
    friend class Entity;
    EntityCategory categoryImpl() const { return EntityCategory::Value; }

    size_t _offset = 0;
    size_t _index = 0;
};

/// Represents a temporary object
class SCATHA_API Temporary: public Object {
public:
    SC_MOVEONLY(Temporary);

    explicit Temporary(size_t id, Scope* parentScope, QualType type);

    /// The ID of this temporary
    size_t id() const { return _id; }

private:
    friend class Entity;
    EntityCategory categoryImpl() const { return EntityCategory::Value; }

    size_t _id;
};

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

    /// \Returns `true` if \p scope is a child scope of this
    bool isChildScope(Scope const* scope) const {
        return _children.contains(scope);
    }

    /// \Returns A View over the children of this scope
    auto children() const { return _children | Opaque; }

    /// \Returns A View over the entities in this scope
    auto entities() const { return _entities | ranges::views::values; }

protected:
    explicit Scope(EntityType entityType,
                   ScopeKind,
                   std::string name,
                   Scope* parent);

private:
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
class SCATHA_API AnonymousScope: public Scope {
public:
    explicit AnonymousScope(ScopeKind scopeKind, Scope* parent);
};

/// Represents the global scope
class SCATHA_API GlobalScope: public Scope {
public:
    explicit GlobalScope();
};

/// # Function

/// Represents the signature, aka parameter types and return type of a function
class SCATHA_API FunctionSignature {
public:
    FunctionSignature() = default;

    explicit FunctionSignature(utl::small_vector<QualType> argumentTypes,
                               QualType returnType):
        _argumentTypes(std::move(argumentTypes)), _returnType(returnType) {}

    Type const* type() const {
        /// Don't have function types yet. Not sure we if we even need them
        /// though
        SC_UNIMPLEMENTED();
    }

    /// Argument types
    std::span<QualType const> argumentTypes() const { return _argumentTypes; }

    /// Argument type at index \p index
    QualType argumentType(size_t index) const { return _argumentTypes[index]; }

    /// Number of arguments
    size_t argumentCount() const { return _argumentTypes.size(); }

    /// Return type
    QualType returnType() const { return _returnType; }

private:
    utl::small_vector<QualType> _argumentTypes;
    QualType _returnType;
};

/// \Returns `true` if \p a and \p b have the same argument types
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
    QualType returnType() const { return _sig.returnType(); }

    /// Argument types
    std::span<QualType const> argumentTypes() const {
        return _sig.argumentTypes();
    }

    /// Argument type at index \p index
    QualType argumentType(size_t index) const {
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

    /// \Returns `true` if this function is a special member function
    bool isSpecialMemberFunction() const { return _smfKind.has_value(); }

    /// \Returns the kind of special member function if this function is a
    /// special member function
    SpecialMemberFunction SMFKind() const { return *_smfKind; }

    /// Set the kind of special member function this function is
    void setSMFKind(SpecialMemberFunction kind) { _smfKind = kind; }

    /// \returns Slot of extern function table.
    ///
    /// Only applicable if this function is extern.
    size_t slot() const { return _slot; }

    /// \returns Index into slot of extern function table.
    ///
    /// Only applicable if this function is extern.
    size_t index() const { return _index; }

    /// The address of this function in the compiled binary
    /// Only has a value if this function is declared externally visible and
    /// program has been compiled
    std::optional<size_t> binaryAddress() const {
        return _haveBinaryAddress ? std::optional(_binaryAddress) :
                                    std::nullopt;
    }

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

    void setBinaryAddress(size_t addr) {
        _haveBinaryAddress = true;
        _binaryAddress = addr;
    }

private:
    friend class Entity;
    EntityCategory categoryImpl() const { return EntityCategory::Value; }

    friend class SymbolTable;
    FunctionSignature _sig;
    OverloadSet* _overloadSet = nullptr;
    FunctionAttribute attrs;
    AccessSpecifier accessSpec = AccessSpecifier::Private;
    std::optional<SpecialMemberFunction> _smfKind;
    FunctionKind _kind = FunctionKind::Native;
    bool _isMember          : 1 = false;
    bool _haveBinaryAddress : 1 = false;
    u16 _slot = 0;
    u32 _index = 0;
    size_t _binaryAddress = 0;
};

/// # Types

size_t constexpr InvalidSize = ~size_t(0);

/// Abstract class representing a type
class SCATHA_API Type: public Scope {
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

private:
    friend class Entity;
    EntityCategory categoryImpl() const { return EntityCategory::Type; }
};

/// Abstract class representing the type of an object
class SCATHA_API ObjectType: public Type {
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

    bool hasTrivialLifetime() const;

private:
    bool hasTrivialLifetimeImpl() const { return true; }

    friend class Type;
    size_t sizeImpl() const { return _size; }
    size_t alignImpl() const { return _align; }

    size_t _size;
    size_t _align;
};

/// Concrete class representing a builtin type
class SCATHA_API BuiltinType: public ObjectType {
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
class SCATHA_API VoidType: public BuiltinType {
public:
    explicit VoidType(Scope* parentScope);
};

/// Abstract class representing an arithmetic type
/// Note that for the purposes of semantic analysis, `BoolType` and `ByteType`
/// are also considered arithmetic types, even though most arithmetic operations
/// are not defined on them
class SCATHA_API ArithmeticType: public BuiltinType {
public:
    /// Number of bits in this type
    size_t bitwidth() const { return _bitwidth; }

    /// `Signed` or `Unsigned`
    /// This is only really meaningful for `IntType`, but very convenient to
    /// have it in the arithmetic interface `BoolType` and `ByteType` are always
    /// `Unsigned`, `FloatType` is always `Signed`
    Signedness signedness() const { return _signed; }

    /// Shorthand for `signedness() == Signed`
    bool isSigned() const { return signedness() == Signedness::Signed; }

    /// Shorthand for `signedness() == Unsigned`
    bool isUnsigned() const { return signedness() == Signedness::Unsigned; }

protected:
    explicit ArithmeticType(EntityType entityType,
                            std::string name,
                            size_t bitwidth,
                            Signedness signedness,
                            Scope* parentScope):
        BuiltinType(entityType,
                    std::move(name),
                    parentScope,
                    utl::ceil_divide(bitwidth, 8),
                    utl::ceil_divide(bitwidth, 8)),
        _signed(signedness),
        _bitwidth(utl::narrow_cast<uint16_t>(bitwidth)) {}

private:
    Signedness _signed;
    uint16_t _bitwidth;
};

/// Concrete class representing type `bool`
class SCATHA_API BoolType: public ArithmeticType {
public:
    explicit BoolType(Scope* parentScope);
};

/// Concrete class representing type `byte`
class SCATHA_API ByteType: public ArithmeticType {
public:
    explicit ByteType(Scope* parentScope);
};

/// Concrete class representing an integral type
class SCATHA_API IntType: public ArithmeticType {
public:
    explicit IntType(size_t bitwidth,
                     Signedness signedness,
                     Scope* parentScope);
};

/// Concrete class representing a floating point type
class SCATHA_API FloatType: public ArithmeticType {
public:
    explicit FloatType(size_t bitwidth, Scope* parentScope);
};

/// Concrete class representing the type of a structure
class SCATHA_API StructureType: public ObjectType {
public:
    explicit StructureType(std::string name,
                           Scope* parentScope,
                           size_t size = InvalidSize,
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

    /// Adds a variable to the list of member variables of this structure
    void addMemberVariable(Variable* var) { _memberVars.push_back(var); }

    ///
    void addSpecialMemberFunction(SpecialMemberFunction kind,
                                  OverloadSet* overloadSet) {
        specialMemberFunctions[kind] = overloadSet;
    }

    ///
    OverloadSet* specialMemberFunction(SpecialMemberFunction kind) const {
        auto itr = specialMemberFunctions.find(kind);
        if (itr != specialMemberFunctions.end()) {
            return itr->second;
        }
        return nullptr;
    }

    ///
    Function* specialLifetimeFunction(SpecialLifetimeFunction kind) const {
        return specialLifetimeFunctions[static_cast<size_t>(kind)];
    }

    /// This should be only accessible by the implementation of
    /// `instantiateEntities()` but it's just to cumbersome to make it private
    void setHasTrivialLifetime(bool value) { _hasTrivialLifetime = value; }

    /// This should be private as well
    void setSpecialLifetimeFunctions(
        std::array<Function*, EnumSize<SpecialLifetimeFunction>> SLF) {
        specialLifetimeFunctions = SLF;
    }

private:
    friend class ObjectType;
    /// Structure type has trivial lifetime if no user defined copy constructor,
    /// move constructor or destructor are present and all member types are
    /// trivial
    bool hasTrivialLifetimeImpl() const { return _hasTrivialLifetime; }

    utl::small_vector<Variable*> _memberVars;
    utl::hashmap<SpecialMemberFunction, OverloadSet*> specialMemberFunctions;
    std::array<Function*, EnumSize<SpecialLifetimeFunction>>
        specialLifetimeFunctions = {};
    mutable bool _hasTrivialLifetime : 1 = false;
};

/// Concrete class representing the type of an array
class SCATHA_API ArrayType: public ObjectType {
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
        _count(count) {
        setBuiltin(elementType->isBuiltin());
    }

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
    friend class ObjectType;
    bool hasTrivialLifetimeImpl() const {
        return elementType()->hasTrivialLifetime();
    }
    static std::string makeName(ObjectType const* elemType, size_t size);

    ObjectType const* elemType;
    Variable* countVar = nullptr;
    size_t _count;
};

/// Represents a reference type
class SCATHA_API ReferenceType: public ObjectType {
public:
    explicit ReferenceType(QualType base, Reference ref);

    /// The type that this `ReferenceType` refers to
    QualType base() const { return _base; }

    /// The reference qualifier of this type
    Reference reference() const { return ref; }

    /// \Returns `true` if this is an explicit reference type
    bool isExplicit() const { return reference() == Reference::Explicit; }

    /// \Returns `true` if this is an implicit reference type
    bool isImplicit() const { return reference() == Reference::Implicit; }

private:
    QualType _base;
    Reference ref;
};

/// # OverloadSet

class SCATHA_API OverloadSet:
    public Entity,
    private utl::small_vector<Function*, 8> {
    using VecBase = utl::small_vector<Function*, 8>;

public:
    SC_MOVEONLY(OverloadSet);

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

private:
    friend class Entity;
    EntityCategory categoryImpl() const { return EntityCategory::Value; }
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

/// # Poison

/// Represents a poison entity, an invalid entity to help spread redundant error
/// messages
class SCATHA_API PoisonEntity: public Entity {
public:
    explicit PoisonEntity(std::string name,
                          EntityCategory cat,
                          Scope* parentScope):
        Entity(EntityType::PoisonEntity, std::move(name), parentScope),
        cat(cat) {}

private:
    friend class Entity;
    EntityCategory categoryImpl() const { return cat; }

    EntityCategory cat;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_ENTITY_H_
