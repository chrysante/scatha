#ifndef SCATHA_SEMA_ENTITY_H_
#define SCATHA_SEMA_ENTITY_H_

#include <array>
#include <concepts>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include <range/v3/view.hpp>
#include <utl/hashmap.hpp>
#include <utl/vector.hpp>

#include <scatha/AST/Fwd.h>
#include <scatha/Common/Base.h>
#include <scatha/Common/Ranges.h>
#include <scatha/Common/SourceLocation.h>
#include <scatha/Common/UniquePtr.h>
#include <scatha/Sema/Fwd.h>
#include <scatha/Sema/QualType.h>

/// Class hierarchy of `Entity`
///
/// ```
/// Entity
/// ├─ Object
/// │  ├─ Variable
/// │  ├─ Property
/// │  └─ Temporary
/// ├─ OverloadSet
/// ├─ Generic
/// ├─ Scope
/// │  ├─ GlobalScope
/// │  ├─ FileScope
/// │  ├─ AnonymousScope
/// │  ├─ Function
/// │  └─ Type
/// │     ├─ ReferenceType
/// │     ├─ FunctionType [Does not exist yet]
/// │     └─ ObjectType
/// │        ├─ BuiltinType
/// │        │  ├─ VoidType
/// │        │  ├─ ArithmeticType
/// │        │  │  ├─ BoolType
/// │        │  │  ├─ ByteType
/// │        │  │  ├─ IntType
/// │        │  │  └─ FloatType
/// │        │  └─ NullPtrType
/// │        ├─ PointerType
/// │        │  ├─ RawPtrType
/// │        │  └─ UniquePtrType
/// │        └─ CompoundType
/// │           ├─ StructType
/// │           └─ ArrayType
/// └─ PoisonEntity
/// ```

#define SC_ASTNODE_DERIVED(Name, Type)                                         \
    template <typename T = ast::Type>                                          \
    T const* Name() const {                                                    \
        return cast<T const*>(astNode());                                      \
    }                                                                          \
    template <typename T = ast::Type>                                          \
    T* Name() {                                                                \
        return cast<T*>(astNode());                                            \
    }

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

    /// The parent scope of this entity. Not all entities have a parent scope so
    /// this may be null.
    /// This is the first and primary parent. Entities may have multiple
    /// parents.
    Scope* parent() { return _parents.front(); }

    /// \overload
    Scope const* parent() const { return _parents.front(); }

    /// The complete list of parent scopes. Entities can have multiple scopes to
    /// make public symbol visible in the global scope and to allow using
    /// directives
    std::span<Scope* const> parents() { return _parents; }

    /// \overload
    std::span<Scope const* const> parents() const { return _parents; }

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

    /// \Returns the corresponding AST node
    ast::ASTNode* astNode() { return _astNode; }

    /// \overload
    ast::ASTNode const* astNode() const { return _astNode; }

    /// \Returns the access spec if this entity is a declaration
    std::optional<AccessSpecifier> accessSpec() const;

protected:
    explicit Entity(EntityType entityType,
                    std::string name,
                    Scope* parent,
                    ast::ASTNode* astNode = nullptr):
        _entityType(entityType),
        _parents({ parent }),
        _names({ std::move(name) }),
        _astNode(astNode) {}

private:
    friend class Scope;
    /// To be used by scope
    void addParent(Scope* parent);

    /// Default implementation of `category()`
    EntityCategory categoryImpl() const {
        return EntityCategory::Indeterminate;
    }

    /// Type ID used by `dyncast`
    EntityType _entityType;
    bool _isBuiltin = false;
    utl::small_vector<Scope*, 2> _parents;
    utl::small_vector<std::string, 1> _names;
    mutable std::string _mangledName;
    ast::ASTNode* _astNode = nullptr;
};

/// Customization point for the `dyncast` facilities
EntityType dyncast_get_type(std::derived_from<Entity> auto const& entity) {
    return entity.entityType();
}

/// Represents an object
class SCATHA_API Object: public Entity {
public:
    Object(Object const&) = delete;

    ~Object();

    /// Set the type of this object.
    void setType(Type const* type) { _type = type; }

    /// Type of this object.
    Type const* type() const { return _type; }

    /// Mutability of this object.
    Mutability mutability() const { return _mut; }

    /// \Returns `true` if this object is mutable
    bool isMut() const { return mutability() == Mutability::Mutable; }

    /// \Returns `true` if this object is const
    bool isConst() const { return !isMut(); }

    /// \Returns the QualType that represents the type of this object.
    /// That is, if this object is a reference, it returns the referred to type,
    /// otherwise returns the type including mutability qualifier
    QualType getQualType() const;

    /// \Returns Constant value if this variable is `const` and has a
    /// const-evaluable initializer `nullptr` otherwise
    Value const* constantValue() const { return constVal.get(); }

    /// Set the constant value of this variable
    void setConstantValue(UniquePtr<Value> value);

protected:
    explicit Object(EntityType entityType,
                    std::string name,
                    Scope* parentScope,
                    Type const* type,
                    Mutability mutability,
                    ast::ASTNode* astNode = nullptr);

    void setMutability(Mutability mut) { _mut = mut; }

private:
    friend class Entity;
    EntityCategory categoryImpl() const { return EntityCategory::Value; }

    Type const* _type;
    Mutability _mut;
    UniquePtr<Value> constVal;
};

/// Common base class of `Variable` and `Property`
class SCATHA_API VarBase: public Object {
public:
    /// The value category of this variable or property. For variables this is
    /// always lvalue but for properties it varies
    ValueCategory valueCategory() const;

protected:
    using Object::Object;
};

/// Represents a variable
class SCATHA_API Variable: public VarBase {
public:
    Variable(Variable const&) = delete;

    explicit Variable(std::string name,
                      Scope* parentScope,
                      ast::ASTNode* astNode,
                      Type const* type = nullptr,
                      Mutability mutability = {});

    /// The AST node that corresponds to this variable
    SC_ASTNODE_DERIVED(declaration, VarDeclBase)

    /// If this variable is a member of a struct, this is the position of this
    /// variable in the struct.
    size_t index() const { return _index; }

    /// Set the index of this variable.
    void setIndex(size_t index) { _index = index; }

    /// For the symbol table
    using Object::setMutability;

private:
    friend class Entity;
    EntityCategory categoryImpl() const { return EntityCategory::Value; }
    friend class VarBase;
    ValueCategory valueCatImpl() const { return ValueCategory::LValue; }

    size_t _index = 0;
};

/// Represents a computed property such as `.count`, `.front` and `.back` member
/// of arrays
class SCATHA_API Property: public VarBase {
public:
    explicit Property(PropertyKind kind,
                      Scope* parentScope,
                      Type const* type,
                      Mutability mut,
                      ValueCategory valueCat);

    /// The kind of property
    PropertyKind kind() const { return _kind; }

private:
    friend class Entity;
    EntityCategory categoryImpl() const { return EntityCategory::Value; }
    friend class VarBase;
    ValueCategory valueCatImpl() const { return _valueCat; }

    PropertyKind _kind;
    ValueCategory _valueCat;
};

/// Represents a temporary object
class SCATHA_API Temporary: public Object {
public:
    explicit Temporary(size_t id, Scope* parentScope, QualType type);

    Temporary(Temporary const&) = delete;

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

    /// Find entities by name within this scope
    std::span<Entity* const> findEntities(std::string_view name) {
        auto ret = std::as_const(*this).findEntities(name);
        return { const_cast<Entity* const*>(ret.data()), ret.size() };
    }

    /// \overload
    std::span<Entity const* const> findEntities(std::string_view name) const;

    /// Find the property \p prop in this scope
    Property* findProperty(PropertyKind prop) {
        return const_cast<Property*>(std::as_const(*this).findProperty(prop));
    }

    /// \overload
    Property const* findProperty(PropertyKind kind) const;

    /// \Returns `true` if \p scope is a child scope of this
    bool isChildScope(Scope const* scope) const {
        return _children.contains(scope);
    }

    /// \Returns A View over the children of this scope
    auto children() const { return _children | Opaque; }

    /// \Returns A View over the entities in this scope
    auto entities() const {
        return _names | ranges::views::values | ranges::views::join;
    }

    /// Add \p entity as a child of this scope. This function is used by the
    /// symbol table and should ideally be private
    void addChild(Entity* entity);

protected:
    explicit Scope(EntityType entityType,
                   ScopeKind,
                   std::string name,
                   Scope* parent,
                   ast::ASTNode* astNode = nullptr);

private:
    friend class SymbolTable;
    friend class Entity;

    /// Callback for `Entity::addAlternateName()` to register the new name in
    /// our symbol table Does nothing if \p child is not a child of this scope
    void addAlternateChildName(Entity* child, std::string name);

private:
    utl::hashset<Scope*> _children;
    utl::hashmap<std::string, utl::small_vector<Entity*, 1>> _names;
    utl::hashmap<PropertyKind, Property*> _properties;
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

/// Represents a file scope
class SCATHA_API FileScope: public Scope {
public:
    explicit FileScope(std::string filename, Scope* parent);
};

/// Scope of symbols imported from a library
class SCATHA_API LibraryScope: public Scope {
public:
    explicit LibraryScope(std::string name, Scope* parent);
};

/// # Function

/// Represents the signature, aka parameter types and return type of a function
class SCATHA_API FunctionSignature {
public:
    FunctionSignature() = default;

    explicit FunctionSignature(utl::small_vector<Type const*> argumentTypes,
                               Type const* returnType):
        _argumentTypes(std::move(argumentTypes)), _returnType(returnType) {}

    Type const* type() const {
        /// Don't have function types yet. Not sure we if we even need them
        /// though
        SC_UNIMPLEMENTED();
    }

    /// Argument types
    std::span<Type const* const> argumentTypes() const {
        return _argumentTypes;
    }

    /// Argument type at index \p index
    Type const* argumentType(size_t index) const {
        return _argumentTypes[index];
    }

    /// Number of arguments
    size_t argumentCount() const { return _argumentTypes.size(); }

    /// \Returns the return type.
    /// \Warning During analysis this could be null if the return type is
    /// not yet deduced.
    Type const* returnType() const { return _returnType; }

private:
    friend class Function; /// To set deduced return type
    utl::small_vector<Type const*> _argumentTypes;
    Type const* _returnType = nullptr;
};

/// \Returns `true` if \p a and \p b have the same argument types
bool argumentsEqual(FunctionSignature const& a, FunctionSignature const& b);

/// Represents a builtin or user defined function
class SCATHA_API Function: public Scope {
public:
    explicit Function(std::string name,
                      Scope* parentScope,
                      FunctionAttribute attrs,
                      ast::ASTNode* astNode):
        Scope(EntityType::Function,
              ScopeKind::Function,
              std::move(name),
              parentScope,
              astNode),
        attrs(attrs) {}

    /// The definition of this function in the AST
    SC_ASTNODE_DERIVED(definition, FunctionDefinition)

    /// \Returns The type of this function.
    Type const* type() const { return signature().type(); }

    /// Set the signature of this function
    void setSignature(FunctionSignature sig) {
        _hasSig = true;
        _sig = std::move(sig);
    }

    /// \Returns The signature of this function.
    FunctionSignature const& signature() const {
        SC_EXPECT(hasSignature());
        return _sig;
    }

    /// \Returns `true` if the signature of this function has been set
    bool hasSignature() const { return _hasSig; }

    /// Return type
    Type const* returnType() const { return signature().returnType(); }

    /// Sets the return type to \p type
    /// Must only be called if the return type of this function is yet unknown
    void setDeducedReturnType(Type const* type);

    /// Argument types
    std::span<Type const* const> argumentTypes() const {
        return signature().argumentTypes();
    }

    /// Argument type at index \p index
    Type const* argumentType(size_t index) const {
        return signature().argumentType(index);
    }

    /// Number of arguments
    size_t argumentCount() const { return signature().argumentCount(); }

    /// Kind of this function
    FunctionKind kind() const { return _kind; }

    void setKind(FunctionKind kind) { _kind = kind; }

    /// \Returns `kind() == FunctionKind::Native`
    bool isNative() const { return kind() == FunctionKind::Native; }

    /// \Returns `kind() == FunctionKind::Generated`
    bool isGenerated() const { return kind() == FunctionKind::Generated; }

    /// \Returns `kind() == FunctionKind::Foreign`
    bool isForeign() const { return kind() == FunctionKind::Foreign; }

    /// \Returns `isForeign() && slot() == svm::BuiltinFunctionSlot`
    bool isBuiltin() const;

    /// \Returns `binaryVisibility() == BinaryVisibility::Export`
    bool isBinaryVisible() const {
        return binaryVisibility() == BinaryVisibility::Export;
    }

    /// \Returns `true` if this is a member function
    bool isMember() const { return _isMember; }

    void setIsMember(bool value = true) { _isMember = value; }

    /// \Returns `true` if this function is a special member function
    bool isSpecialMemberFunction() const { return _isSMF; }

    /// \Returns the kind of special member function if this function is a
    /// special member function
    SpecialMemberFunction SMFKind() const {
        SC_EXPECT(_isSMF);
        return _smfKind;
    }

    /// Set the kind of special member function this function is
    void setSMFKind(SpecialMemberFunction kind) {
        _isSMF = true;
        _smfKind = kind;
    }

    /// \Returns `true` if this function is a special lifetime function
    bool isSpecialLifetimeFunction() const { return _isSLF; }

    /// \Returns the kind of special lifetime function if this function is a
    /// special lifetime function
    SpecialLifetimeFunction SLFKind() const {
        SC_EXPECT(_isSLF);
        return _slfKind;
    }

    /// Set the kind of special member function this function is
    void setSLFKind(SpecialLifetimeFunction kind) {
        _isSLF = true;
        _slfKind = kind;
    }

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
        return _hasBinaryAddress ? std::optional(_binaryAddress) : std::nullopt;
    }

    void setBinaryAddress(size_t addr) {
        _hasBinaryAddress = true;
        _binaryAddress = addr;
    }

    /// See Sema/Fwd.h
    AccessSpecifier accessSpecifier() const { return accessSpec; }

    void setAccessSpecifier(AccessSpecifier spec) { accessSpec = spec; }

    /// See Sema/Fwd.h
    BinaryVisibility binaryVisibility() const { return binaryVis; }

    void setBinaryVisibility(BinaryVisibility vis) { binaryVis = vis; }

    /// \returns Bitfield of function attributes
    FunctionAttribute attributes() const { return attrs; }

    /// Set attribute \p attr to `true`.
    void setAttribute(FunctionAttribute attr) { attrs |= attr; }

    /// Set attribute \p attr to `false`.
    void removeAttribute(FunctionAttribute attr) { attrs &= ~attr; }

private:
    friend class Entity;
    EntityCategory categoryImpl() const { return EntityCategory::Value; }

    friend class SymbolTable;
    FunctionSignature _sig;
    FunctionAttribute attrs;                                         // 4 bytes
    AccessSpecifier accessSpec       : 4 = AccessSpecifier::Private; // 4 bits
    BinaryVisibility binaryVis       : 4 = BinaryVisibility::Internal; // 4 bits
    SpecialMemberFunction _smfKind   : 4 = {};                         // 4 bits
    SpecialLifetimeFunction _slfKind : 4 = {};                         // 4 bits
    FunctionKind _kind               : 4 = FunctionKind::Native;       // 4 bits
    bool _isSMF                      : 1 = false;
    bool _isSLF                      : 1 = false;
    bool _hasSig                     : 1 = false;
    bool _isMember                   : 1 = false;
    bool _hasBinaryAddress           : 1 = false;
    /// For foreign functions
    u16 _slot = 0;
    u32 _index = 0;
    /// For binary visible functions to be set after compilation
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

    /// \Returns `size() != InvalidSize`
    /// Specifically this returns `true` for `void` and dynamic array types
    bool isComplete() const;

    ///
    bool isDefaultConstructible() const;

    ///
    bool hasTrivialLifetime() const;

protected:
    explicit Type(EntityType entityType,
                  ScopeKind scopeKind,
                  std::string name,
                  Scope* parent,
                  ast::ASTNode* astNode = nullptr):
        Scope(entityType, scopeKind, std::move(name), parent, astNode) {}

private:
    friend class Entity;
    EntityCategory categoryImpl() const { return EntityCategory::Type; }

    size_t sizeImpl() const { return InvalidSize; }
    size_t alignImpl() const { return InvalidSize; }
    bool isDefaultConstructibleImpl() const { return true; }
    bool hasTrivialLifetimeImpl() const { return true; }
};

/// Abstract class representing the type of an object
class SCATHA_API ObjectType: public Type {
public:
    void setSize(size_t value) { _size = value; }

    void setAlign(size_t value) { _align = value; }

    ///
    void addSpecialMemberFunction(SpecialMemberFunction kind,
                                  Function* function) {
        SMFs[kind].push_back(function);
    }

    ///
    std::span<Function* const> specialMemberFunctions(
        SpecialMemberFunction kind) const {
        auto itr = SMFs.find(kind);
        if (itr != SMFs.end()) {
            return itr->second;
        }
        return {};
    }

    ///
    Function* specialLifetimeFunction(SpecialLifetimeFunction kind) const {
        return SLFs[static_cast<size_t>(kind)];
    }

    /// These functions should be only accessible by the implementation of
    /// `instantiateEntities()` but it's just to cumbersome to make it private
    void setIsDefaultConstructible(bool value) {
        _isDefaultConstructible = value;
    }

    /// See above
    void setHasTrivialLifetime(bool value) { _hasTrivialLifetime = value; }

    /// See above
    void setSpecialLifetimeFunctions(
        std::array<Function*, EnumSize<SpecialLifetimeFunction>> SLFs) {
        this->SLFs = SLFs;
    }

protected:
    explicit ObjectType(EntityType entityType,
                        ScopeKind scopeKind,
                        std::string name,
                        Scope* parent,
                        size_t size,
                        size_t align,
                        ast::ASTNode* astNode = nullptr):
        Type(entityType, scopeKind, std::move(name), parent, astNode),
        _size(size),
        _align(align) {}

private:
    friend class Type;
    friend class StructType;
    size_t sizeImpl() const { return _size; }
    size_t alignImpl() const { return _align; }
    bool isDefaultConstructible() const { return _isDefaultConstructible; }

    size_t _size;
    size_t _align;
    utl::hashmap<SpecialMemberFunction, utl::small_vector<Function*, 1>> SMFs;
    std::array<Function*, EnumSize<SpecialLifetimeFunction>> SLFs = {};
    bool _hasTrivialLifetime     : 1 = true; // Only used by structs
    bool _isDefaultConstructible : 1 = true;
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
                   ScopeKind::Type,
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
                            Scope* parentScope);

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

/// Concrete class the type of the `null` literal. This type only has a single
/// value `null`
class SCATHA_API NullPtrType: public BuiltinType {
public:
    explicit NullPtrType(Scope* parent);
};

/// ## About trivial lifetime
/// Types are said to have _trivial lifetime_ if the destructor, the copy
/// constructor and the move constructor are _trivial_. The lifetime functions
/// of builtin types are always trivial. For struct types a lifetime function is
/// trivial if it is not user defined and the corresponding lifetime function is
/// trivial for all data members. For array types a lifetime function is trivial
/// if the corresponding lifetime function of the element type is trivial.

/// Abstract base class of `StructType` and `ArrayType`
class SCATHA_API CompoundType: public ObjectType {
protected:
    using ObjectType::ObjectType;
};

/// Concrete class representing the type of a structure
class SCATHA_API StructType: public CompoundType {
public:
    explicit StructType(std::string name,
                        Scope* parentScope,
                        ast::ASTNode* astNode,
                        size_t size = InvalidSize,
                        size_t align = InvalidSize):
        CompoundType(EntityType::StructType,
                     ScopeKind::Type,
                     std::move(name),
                     parentScope,
                     size,
                     align,
                     astNode) {}

    /// The AST node that defines this type
    SC_ASTNODE_DERIVED(definition, StructDefinition)

    /// The member variables of this type in the order of declaration.
    std::span<Variable* const> memberVariables() { return _memberVars; }

    /// \overload
    std::span<Variable const* const> memberVariables() const {
        return _memberVars;
    }

    /// \Returns a view over the member types in this struct
    auto members() const {
        return memberVariables() |
               ranges::views::transform([](auto* var) { return var->type(); });
    }

    /// Adds a variable to the list of member variables of this structure
    void addMemberVariable(Variable* var) { _memberVars.push_back(var); }

private:
    friend class Type;
    /// Structure type has trivial lifetime if no user defined copy constructor,
    /// move constructor or destructor are present and all member types are
    /// trivial
    bool hasTrivialLifetimeImpl() const { return _hasTrivialLifetime; }

    utl::small_vector<Variable*> _memberVars;
};

/// Concrete class representing the type of an array
class SCATHA_API ArrayType: public CompoundType {
public:
    static constexpr size_t DynamicCount = ~size_t(0);

    explicit ArrayType(ObjectType* elementType, size_t count):
        CompoundType(EntityType::ArrayType,
                     ScopeKind::Type,
                     makeName(elementType, count),
                     elementType->parent(),
                     count != DynamicCount ? count * elementType->size() :
                                             InvalidSize,
                     elementType->align()),
        elemType(elementType),
        _count(count) {
        setBuiltin(elementType->isBuiltin());
    }

    /// Type of the elements in this array
    ObjectType* elementType() { return elemType; }

    /// \overload
    ObjectType const* elementType() const { return elemType; }

    /// Number of elements in this array
    size_t count() const { return _count; }

    /// Shorthand for `count() == DynamicCount`
    bool isDynamic() const { return count() == DynamicCount; }

private:
    static std::string makeName(ObjectType const* elemType, size_t size);

    friend class Type;
    bool hasTrivialLifetimeImpl() const {
        return elementType()->hasTrivialLifetime();
    }

    ObjectType* elemType;
    size_t _count;
};

/// Common base class of `PointerType` and `ReferenceType`
class SCATHA_API PtrRefTypeBase {
public:
    /// The type referred to by the pointer or reference
    QualType base() const { return _base; }

protected:
    PtrRefTypeBase(QualType type): _base(type) {}

private:
    QualType _base;
};

/// Abstract base class of raw pointer and unique pointer
class SCATHA_API PointerType: public ObjectType, public PtrRefTypeBase {
protected:
    explicit PointerType(EntityType entityType,
                         QualType base,
                         std::string name);
};

/// Represents a raw pointer type
class SCATHA_API RawPtrType: public PointerType {
public:
    explicit RawPtrType(QualType base);
};

/// Represents a unique pointer type
class SCATHA_API UniquePtrType: public PointerType {
public:
    explicit UniquePtrType(QualType base);

private:
    friend class Type;
    bool isDefaultConstructibleImpl() const { return true; }
    bool hasTrivialLifetimeImpl() const { return false; }
};

/// Represents a reference type
class SCATHA_API ReferenceType: public Type, public PtrRefTypeBase {
public:
    explicit ReferenceType(QualType base);

private:
    friend class Type;
    size_t sizeImpl() const { return 0; }
    size_t alignImpl() const { return 0; }
    bool isDefaultConstructibleImpl() const { return false; }
};

/// # OverloadSet

/// Groups a set of functions to perform overload resolution. Overload sets are
/// formed when a function is called and consist of all functions that are found
/// by name lookup at the call site
class SCATHA_API OverloadSet:
    public Entity,
    private utl::small_vector<Function*> {
public:
    explicit OverloadSet(SourceRange loc,
                         std::string name,
                         utl::small_vector<Function*> functions):
        Entity(EntityType::OverloadSet, std::move(name), nullptr),
        small_vector(std::move(functions)),
        loc(loc) {}

    OverloadSet(OverloadSet const&) = delete;

    /// \Returns the function in this overload set that exactly matches the
    /// parameter types \p paramTypes
    Function* find(std::span<Type const* const> paramTypes) {
        return const_cast<Function*>(std::as_const(*this).find(paramTypes));
    }

    /// \overload for const
    Function const* find(std::span<Type const* const> paramTypes) const;

    /// \overload
    static Function* find(std::span<Function* const> set,
                          std::span<Type const* const> paramTypes) {
        std::span<Function const* const> cSet(set.data(), set.size());
        return const_cast<Function*>(find(cSet, paramTypes));
    }

    /// \overload
    static Function const* find(std::span<Function const* const> set,
                                std::span<Type const* const> paramTypes);

    /// The location where this overload set ist formed
    SourceRange sourceRange() const { return loc; }

    /// Inherit interface from `utl::vector`
    using small_vector::begin;
    using small_vector::empty;
    using small_vector::end;
    using small_vector::size;
    using small_vector::operator[];
    using small_vector::back;
    using small_vector::front;

private:
    friend class Entity;
    EntityCategory categoryImpl() const { return EntityCategory::Value; }

    SourceRange loc;
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

#undef SC_ASTNODE_DERIVED

#endif // SCATHA_SEMA_ENTITY_H_
