#ifndef SCATHA_SEMA_ENTITY_H_
#define SCATHA_SEMA_ENTITY_H_

#include <array>
#include <concepts>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>
#include <utl/tiny_ptr_vector.hpp>
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
///     Entity
///     ├─ Object
///     │  ├─ VarBase
///     │  │  ├─ Variable
///     │  │  └─ Property
///     │  └─ Temporary
///     ├─ OverloadSet
///     ├─ Generic
///     ├─ Scope
///     │  ├─ GlobalScope
///     │  ├─ FileScope
///     │  ├─ AnonymousScope
///     │  ├─ Function
///     │  └─ Type
///     │     ├─ ReferenceType
///     │     ├─ FunctionType
///     │     └─ ObjectType
///     │        ├─ BuiltinType
///     │        │  ├─ VoidType
///     │        │  ├─ ArithmeticType
///     │        │  │  ├─ BoolType
///     │        │  │  ├─ ByteType
///     │        │  │  ├─ IntType
///     │        │  │  └─ FloatType
///     │        │  ├─ NullPtrType
///     │        │  └─ PointerType
///     │        │     ├─ RawPtrType
///     │        │     └─ UniquePtrType
///     │        └─ CompoundType
///     │           ├─ StructType
///     │           ├─ ProtocolType
///     │           └─ ArrayType
///     ├─ TypeDeductionQualifier
///     └─ PoisonEntity

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
    std::string_view name() const { return _name; }

    /// Sets the primary name of this entity to \p name
    void setName(std::string name) { _name = std::move(name); }

    /// \Returns `true` if this entity is unnamed
    bool isAnonymous() const { return name().empty(); }

    /// \Returns `true` if this entities is a builtin
    bool isBuiltin() const { return _isBuiltin; }

    ///
    void setBuiltin(bool value = true) { _isBuiltin = value; }

    /// \Returns the access control of this entity. Only meaningful if this
    /// entity is a function, type, or variable.
    /// \Warning this function will trap if this entity has no access control
    AccessControl accessControl() const {
        SC_EXPECT(hasAccessControl());
        return accessCtrl;
    }

    /// \Returns `true` if it is safe to call `accessControl()`
    bool hasAccessControl() const { return accessCtrl != InvalidAccessControl; }

    /// \Returns `accessControl() == Private`
    bool isPrivate() const { return accessControl() == AccessControl::Private; }

    /// \Returns `accessControl() == Internal`
    bool isInternal() const {
        return accessControl() == AccessControl::Internal;
    }

    /// \Returns `accessControl() == Public`
    bool isPublic() const { return accessControl() == AccessControl::Public; }

    /// Should be used by instantiation and deserialization
    void setAccessControl(AccessControl ctrl) { accessCtrl = ctrl; }

    /// \Returns `true` if this entity is accessible by name lookup
    bool isVisible() const { return _isVisible; }

    /// Set whether this entity shall be accessible by name lookup
    void setVisible(bool value = true) { _isVisible = value; }

    /// The parent scope of this entity. Not all entities have a parent scope so
    /// this may be null.
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

    /// \Returns the corresponding AST node
    ast::ASTNode* astNode() { return _astNode; }

    /// \overload
    ast::ASTNode const* astNode() const { return _astNode; }

    /// \Returns the list of aliases to this entity
    std::span<Alias* const> aliases() { return _aliases; }

    /// \overload
    std::span<Alias const* const> aliases() const { return _aliases; }

protected:
    explicit Entity(EntityType entityType, std::string name, Scope* parent,
                    ast::ASTNode* astNode = nullptr);

private:
    friend class Scope;
    /// To be used by scope
    void setParent(Scope* parent);

    /// Default implementation of `category()`
    EntityCategory categoryImpl() const {
        return EntityCategory::Indeterminate;
    }

    friend class SymbolTable;
    /// To be used by symbol table
    void addAlias(Alias* alias);

    friend EntityType get_rtti(Entity const& entity) {
        return entity.entityType();
    }

    /// Type ID used by `dyncast`
    EntityType _entityType;
    bool _isBuiltin = false;
    bool _isVisible = true;
    AccessControl accessCtrl = InvalidAccessControl;
    Scope* _parent = nullptr;
    std::string _name;
    utl::tiny_ptr_vector<Alias*> _aliases;
    ast::ASTNode* _astNode = nullptr;
};

/// Temporary function. We will remove this function after we restructure the
/// entity hierarchy such that `Function` derives from `Object`
Type const* getEntityType(Entity const& entity);

/// Represents an object
class SCATHA_API Object: public Entity {
public:
    Object(Object const&) = delete;

    ~Object();

    /// Type of this object.
    Type const* type() const { return _type; }

    /// Mutability of this object.
    Mutability mutability() const { return _mut; }

    /// \Returns `true` if this object is mutable
    bool isMut() const { return mutability() == Mutability::Mutable; }

    /// \Returns `true` if this object is const
    bool isConst() const { return !isMut(); }

    ///
    PointerBindMode bindMode() const { return _bindMode; }

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
    explicit Object(EntityType entityType, std::string name, Scope* parentScope,
                    Type const* type, Mutability mutability,
                    PointerBindMode bindMode, ast::ASTNode* astNode);

private:
    friend class Entity;
    EntityCategory categoryImpl() const { return EntityCategory::Value; }

    friend class SymbolTable; // To set the type in two phase initialization
    Type const* _type;
    Mutability _mut;
    PointerBindMode _bindMode;
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

/// Common base class of `Variable` and `BaseClassObject`
class SCATHA_API RecordElement {
public:
    /// \Returns the position of this element in the structure
    size_t index() const { return _index; }

    /// Necessary for symbol table deserialization
    void setIndex(size_t index) { _index = (uint16_t)index; }

    /// \Returns the byte offset of this element in the parent structure
    size_t byteOffset() const { return _byteOffset; }

    ///
    void setByteOffset(size_t offset) { _byteOffset = (uint32_t)offset; }

    ///
    Object& asObject() { return reinterpret_cast<Object&>(*(this + 1)); }

    /// \overload
    Object const& asObject() const {
        return reinterpret_cast<Object const&>(*(this + 1));
    }

    ///
    Type const* type() const { return asObject().type(); }

private:
    friend class RecordType;
    friend class StructType;

    uint16_t _index = 0;
    uint32_t _byteOffset = 0;
};

/// Represents a local, global or struct member variable
/// \Warning `RecordElement` must be the first base class to share the same
/// address
class SCATHA_API Variable: public RecordElement, public VarBase {
public:
    Variable(Variable const&) = delete;

    explicit Variable(std::string name, Scope* parentScope,
                      ast::ASTNode* astNode, AccessControl accessControl,
                      Type const* type = nullptr, Mutability mutability = {});

    /// The AST node that corresponds to this variable
    SC_ASTNODE_DERIVED(declaration, VarDeclBase)

    /// \Returns `true` if this variable is a global or static struct data
    /// member
    bool isStatic() const;

    using Object::type;

private:
    friend class Entity;
    EntityCategory categoryImpl() const { return EntityCategory::Value; }
    friend class VarBase;
    ValueCategory valueCatImpl() const { return ValueCategory::LValue; }
};

/// Represents a base class object
/// \Warning `RecordElement` must be the first base class to share the same
/// address
class SCATHA_API BaseClassObject: public RecordElement, public Object {
public:
    BaseClassObject(BaseClassObject const&) = delete;

    explicit BaseClassObject(std::string name, Scope* parentScope,
                             ast::ASTNode* astNode, AccessControl accessControl,
                             RecordType const* type = nullptr);

    /// The AST node that corresponds to this variable
    SC_ASTNODE_DERIVED(declaration, BaseClassDeclaration)

    ///
    RecordType const* type() const;

private:
    friend class Entity;
    EntityCategory categoryImpl() const { return EntityCategory::Value; }
    ValueCategory valueCatImpl() const { return ValueCategory::LValue; }
};

/// Represents a computed property such as `.count`, `.front` and `.back` member
/// of arrays
class SCATHA_API Property: public VarBase {
public:
    explicit Property(PropertyKind kind, Scope* parentScope, Type const* type,
                      Mutability mut, PointerBindMode bindMode,
                      ValueCategory valueCat, AccessControl accessControl,
                      ast::ASTNode* astNode);

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
    explicit Temporary(size_t id, Scope* parentScope, QualType type,
                       ast::ASTNode* node);

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
    utl::tiny_ptr_vector<Entity*> findEntities(std::string_view name,
                                               bool findHiddenEntities = false);

    /// \overload
    utl::tiny_ptr_vector<Entity const*> findEntities(
        std::string_view name, bool findHiddenEntities = false) const;

    /// Find the property \p prop in this scope
    Property* findProperty(PropertyKind prop) {
        return const_cast<Property*>(std::as_const(*this).findProperty(prop));
    }

    /// \overload
    Property const* findProperty(PropertyKind kind) const;

    /// \Returns a list of the functions in this scope with name \p name
    utl::small_vector<Function*> findFunctions(std::string_view name);

    /// \overload
    utl::small_vector<Function const*> findFunctions(
        std::string_view name) const;

    /// \Returns `true` if \p scope is a child scope of this
    bool isChildScope(Scope const* scope) const {
        return _children.contains(scope);
    }

    /// \Returns A View over the children of this scope
    std::span<Scope* const> children() { return _children.values(); }

    /// \overload
    std::span<Scope const* const> children() const {
        return _children.values();
    }

    /// \Returns A View over the entities in this scope
    auto entities() {
        return _names.values() | ranges::views::values | ranges::views::join;
    }

    /// \overload
    auto entities() const {
        return _names.values() | ranges::views::values | ranges::views::join |
               ToConstAddress;
    }

    /// Add \p entity as a child of this scope. This function is used by
    /// `Entity` and the symbol table and should ideally be private, but we need
    /// access to it in free functions in the implementation of symbol table
    void addChild(Entity* entity);

    /// Removed \p entity from this scope. This does not deallocate the entity
    /// because scopes don't own their children.
    void removeChild(Entity* entity);

protected:
    explicit Scope(EntityType entityType, ScopeKind, std::string name,
                   Scope* parent, ast::ASTNode* astNode = nullptr);

private:
    friend class SymbolTable;
    friend class Entity;

    template <typename E, typename S>
    static utl::tiny_ptr_vector<E*> findEntitiesImpl(S* self,
                                                     std::string_view name,
                                                     bool findHidden);

    utl::hashset<Scope*> _children;
    utl::hashmap<std::string, utl::tiny_ptr_vector<Entity*>> _names;
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
    explicit FileScope(size_t index, std::string filename, Scope* parent);

    /// \Returns the index of the file
    size_t index() const { return _index; }

private:
    size_t _index;
};

/// Abstract base class of `NativeLibrary` and `ForeignLibrary`
class SCATHA_API Library: public Scope {
public:
    /// \Returns a view over the libraries that this library depends on
    std::span<Library* const> dependencies() { return deps; }

    /// \overload
    std::span<Library const* const> dependencies() const { return deps; }

    ///
    void setDependencies(std::span<Library* const> libs) {
        deps = libs | ToSmallVector<2>;
    }

protected:
    explicit Library(EntityType entityType, std::string name, Scope* parent);

private:
    friend class Entity;

    EntityCategory categoryImpl() const { return EntityCategory::Namespace; }

    utl::small_vector<Library*, 2> deps;
};

/// Scope of symbols imported from a library
class SCATHA_API NativeLibrary: public Library {
public:
    explicit NativeLibrary(std::string name, std::filesystem::path path,
                           Scope* parent);

    /// \Returns the resolved location of the library
    std::filesystem::path const& path() const { return _path; }

private:
    std::filesystem::path _path;
};

/// Represents an imported foreign library. Does not contain any child symbols
class SCATHA_API ForeignLibrary: public Library {
public:
    explicit ForeignLibrary(std::string name, std::filesystem::path file,
                            Scope* parent);

    /// \Returns the path of the shared library file
    std::filesystem::path const& file() const { return _file; }

private:
    std::filesystem::path _file;
};

/// # Function

/// Represents a builtin or user defined function
class SCATHA_API Function: public Scope {
public:
    explicit Function(std::string name, FunctionType const* type,
                      Scope* parentScope, FunctionAttribute attrs,
                      ast::ASTNode* astNode, AccessControl accessControl);

    /// The definition of this function in the AST
    SC_ASTNODE_DERIVED(definition, FunctionDefinition)

    /// \Returns The type of this function.
    FunctionType const* type() const { return _type; }

    /// Return type
    Type const* returnType() const;

    /// Argument types
    std::span<Type const* const> argumentTypes() const;

    /// Argument type at index \p index
    Type const* argumentType(size_t index) const;

    /// Number of arguments
    size_t argumentCount() const;

    /// Kind of this function, i.e. `Native`, `Generated` or `Foreign`
    FunctionKind kind() const { return _kind; }

    void setKind(FunctionKind kind) { _kind = kind; }

    /// \Returns `kind() == FunctionKind::Native`
    bool isNative() const { return kind() == FunctionKind::Native; }

    /// \Returns `kind() == FunctionKind::Generated`
    bool isGenerated() const { return kind() == FunctionKind::Generated; }

    /// \Returns `kind() == FunctionKind::Foreign`
    bool isForeign() const { return kind() == FunctionKind::Foreign; }

    /// \Returns `true` if this function is an abstract declaration, i.e., in a
    /// protocol
    bool isAbstract() const { return _isAbstract; }

    /// Mark this function as abstract declaration
    void markAbstract(bool value = true) { _isAbstract = value; }

    /// Sets the kind of special member function. May only be called if this
    /// function is a special member function
    void setSMFKind(SMFKind kind) { _smfKind = kind; }

    /// \Returns the kind of special member functions if this function is a
    /// special member or `std::nullopt`
    std::optional<SMFKind> smfKind() const {
        return _smfKind == SMFKind{ 0xFF } ? std::nullopt :
                                             std::optional(_smfKind);
    }

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
    FunctionType const* _type;
    FunctionAttribute attrs;                           // 4 bytes
    SMFKind _smfKind = SMFKind{ 0xFF };                // 1 byte
    FunctionKind _kind     : 4 = FunctionKind::Native; // 4 bits
    bool _hasSig           : 1 = false;
    bool _isMember         : 1 = false;
    bool _hasBinaryAddress : 1 = false;
    bool _isAbstract       : 1 = false;
    /// For binary visible functions to be set after compilation
    size_t _binaryAddress = 0;
};

/// # Types

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

    /// Convenience method
    /// \Returns `isComplete() || isa<VoidType>(this)`
    bool isCompleteOrVoid() const;

    ///
    bool hasTrivialLifetime() const;

protected:
    explicit Type(EntityType entityType, ScopeKind scopeKind, std::string name,
                  Scope* parent, ast::ASTNode* astNode,
                  AccessControl accessControl);

private:
    friend class Entity;
    EntityCategory categoryImpl() const { return EntityCategory::Type; }

    size_t sizeImpl() const { return InvalidSize; }
    size_t alignImpl() const { return InvalidSize; }
};

/// Represents the signature, aka parameter types and return type of a function
class SCATHA_API FunctionType: public Type {
public:
    FunctionType(std::span<Type const* const> argumentTypes,
                 Type const* returnType);

    FunctionType(utl::small_vector<Type const*> argumentTypes,
                 Type const* returnType);

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
    utl::small_vector<Type const*> _argumentTypes;
    Type const* _returnType = nullptr;
};

/// Abstract class representing the type of an object
class SCATHA_API ObjectType: public Type {
public:
    ~ObjectType();

    ///
    void setSize(size_t value) { _size = value; }

    ///
    void setAlign(size_t value) { _align = value; }

    ///
    void setLifetimeMetadata(LifetimeMetadata md);

    /// \Returns `true` if `setLifetimeMetadata` has been called.
    /// Eventually this will be true for all object types. During instantiation
    /// however this may return false. This function is used to guard against
    /// premature lifetime analysis of derived types like arrays.
    bool hasLifetimeMetadata() const { return lifetimeMD != nullptr; }

    /// \Returns the lifetime metadata associated with this type.
    /// \Pre Lifetime of this type must have been analyzed
    LifetimeMetadata const& lifetimeMetadata() const {
        SC_EXPECT(hasLifetimeMetadata());
        return *lifetimeMD;
    }

protected:
    explicit ObjectType(EntityType entityType, ScopeKind scopeKind,
                        std::string name, Scope* parent, size_t size,
                        size_t align, ast::ASTNode* astNode,
                        AccessControl accessControl);

private:
    friend class Type;
    friend class StructType;
    size_t sizeImpl() const { return _size; }
    size_t alignImpl() const { return _align; }

    size_t _size;
    size_t _align;
    std::unique_ptr<LifetimeMetadata> lifetimeMD;
};

/// Concrete class representing a builtin type
class SCATHA_API BuiltinType: public ObjectType {
protected:
    explicit BuiltinType(EntityType entityType, std::string name,
                         Scope* parentScope, size_t size, size_t align,
                         AccessControl accessControl);
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
    explicit ArithmeticType(EntityType entityType, std::string name,
                            size_t bitwidth, Signedness signedness,
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
    explicit IntType(size_t bitwidth, Signedness signedness,
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

/// Abstract base class of `RecordType` and `ArrayType`
class SCATHA_API CompoundType: public ObjectType {
protected:
    using ObjectType::ObjectType;
};

/// Abstract base class of `StructType` and `Protocol`
class SCATHA_API RecordType: public CompoundType {
public:
    ~RecordType();

    /// The AST node that defines this type
    SC_ASTNODE_DERIVED(definition, RecordDefinition)

    /// \Returns a view over all user defined and compiler generated
    /// constructors. This field will be set after `analyzeLifetime()` has been
    /// called on this type
    std::span<Function* const> constructors() const { return ctors; }

    /// Shall only called be called by `analyzeLifetime()`
    void setConstructors(std::span<Function* const> ctors) {
        this->ctors = ctors | ToSmallVector<>;
    }

    /// All conforming protocols objects, base struct objects and member
    /// variables
    std::span<RecordElement* const> elements() { return _elements; }

    /// \overload
    std::span<RecordElement const* const> elements() const { return _elements; }

    /// All conforming protocols, structs and member variable types
    auto elementTypes() const {
        return elements() | ranges::views::transform(&RecordElement::type);
    }

    /// The base objects of protocol type of this type in the order of
    /// declaration.
    std::span<BaseClassObject* const> conformingProtocolObjects() {
        return elementsImpl<BaseClassObject>(*this, 0, _structBaseBegin);
    }

    /// \overload
    std::span<BaseClassObject const* const> conformingProtocolObjects() const {
        return elementsImpl<BaseClassObject const>(*this, 0, _structBaseBegin);
    }

    /// The base objects of struct type of this type in the order of
    /// declaration.
    std::span<BaseClassObject* const> baseStructObjects() {
        return elementsImpl<BaseClassObject>(*this, _structBaseBegin,
                                             _variableBegin);
    }

    /// \overload
    std::span<BaseClassObject const* const> baseStructObjects() const {
        return elementsImpl<BaseClassObject const>(*this, _structBaseBegin,
                                                   _variableBegin);
    }

    /// The base objects of this type in the order of declaration, except that
    /// protocols precede structs
    std::span<BaseClassObject* const> baseObjects() {
        return elementsImpl<BaseClassObject>(*this, 0, _variableBegin);
    }

    /// \overload
    std::span<BaseClassObject const* const> baseObjects() const {
        return elementsImpl<BaseClassObject const>(*this, 0, _variableBegin);
    }

    /// \Returns a view over the base types of this struct
    template <typename P = ProtocolType>
    auto conformingProtocols() const {
        return conformingProtocolObjects() |
               ranges::views::transform(
                   [](auto* obj) { return cast<P const*>(obj->type()); });
    }

    /// \Returns a view over the base struct of this struct
    template <typename S = StructType>
    auto baseStructs() const {
        return baseStructObjects() | ranges::views::transform([](auto* obj) {
            return cast<S const*>(obj->type());
        });
    }

    /// \Returns a view over the base types of this struct
    auto baseTypes() const {
        return baseObjects() | ranges::views::transform([](auto* obj) {
            return cast<RecordType const*>(obj->type());
        });
    }

    /// Adds a variable to the end of the list of member variables of this
    /// structure
    void pushBaseObject(BaseClassObject* obj);

    ///
    void setVTable(std::unique_ptr<VTable> vtable);

    /// \Returns this types vtable
    VTable* vtable() { return _vtable.get(); }

    /// \overload
    VTable const* vtable() const { return _vtable.get(); }

    ///
    bool isEmpty() const { return _isEmpty; }

    ///
    void setIsEmpty(bool value = true) { _isEmpty = value; }

    /// Sets the element (base class or variable) of this structure at index \p
    /// index
    void setElement(size_t index, Object* obj);

protected:
    explicit RecordType(EntityType entityType, std::string name,
                        Scope* parentScope, ast::ASTNode* astNode, size_t size,
                        size_t align, AccessControl accessControl);

private:
    friend class StructType;

    template <typename T>
    static std::span<T* const> elementsImpl(auto& This, size_t begin,
                                            size_t end) {
        auto* ptr = reinterpret_cast<T* const*>(This._elements.data());
        return { ptr + begin, ptr + end };
    }

    std::unique_ptr<VTable> _vtable;
    utl::small_vector<RecordElement*> _elements;
    bool _isEmpty = false;
    uint16_t _structBaseBegin = 0;
    uint16_t _variableBegin = 0;
    utl::small_vector<Function*, 4> ctors;
};

/// Concrete class representing the type of a structure
class SCATHA_API StructType: public RecordType {
public:
    explicit StructType(std::string name, Scope* parentScope,
                        ast::ASTNode* astNode, size_t size, size_t align,
                        AccessControl accessControl):
        RecordType(EntityType::StructType, std::move(name), parentScope,
                   astNode, size, align, accessControl) {}

    /// The AST node that defines this type
    SC_ASTNODE_DERIVED(definition, StructDefinition)

    /// The member variables of this type in the order of declaration.
    std::span<Variable* const> memberVariables() {
        return elementsImpl<Variable>(*this, _variableBegin, _elements.size());
    }

    /// \overload
    std::span<Variable const* const> memberVariables() const {
        return elementsImpl<Variable const>(*this, _variableBegin,
                                            _elements.size());
    }

    /// \Returns a view over the member types in this struct
    auto memberTypes() const {
        return memberVariables() | ranges::views::transform(&Object::type);
    }

    /// \Returns a view over the non-protocol base class objects and member
    /// variables
    std::span<RecordElement* const> concreteElements() {
        return elementsImpl<RecordElement>(*this, _structBaseBegin,
                                           _elements.size());
    }

    /// \overload
    std::span<RecordElement const* const> concreteElements() const {
        return elementsImpl<RecordElement const>(*this, _structBaseBegin,
                                                 _elements.size());
    }

    /// \Returns a view over the non-protocol base class types and member types
    auto concreteElementTypes() const {
        return concreteElements() |
               ranges::views::transform(&RecordElement::type);
    }

    /// Adds a variable to the end of the list of member variables of this
    /// structure
    void pushMemberVariable(Variable* var);
};

/// Concrete class representing the type of a protocol
class SCATHA_API ProtocolType: public RecordType {
public:
    explicit ProtocolType(std::string name, Scope* parentScope,
                          ast::ASTNode* astNode, AccessControl accessControl):
        RecordType(EntityType::ProtocolType, std::move(name), parentScope,
                   astNode, InvalidSize, InvalidSize, accessControl) {}

    /// The AST node that defines this type
    SC_ASTNODE_DERIVED(definition, ProtocolDefinition)
};

/// Concrete class representing the type of an array
class SCATHA_API ArrayType: public CompoundType {
public:
    static constexpr size_t DynamicCount = ~size_t(0);

    explicit ArrayType(ObjectType* elementType, size_t count);

    /// Type of the elements in this array
    ObjectType* elementType() { return elemType; }

    /// \overload
    ObjectType const* elementType() const { return elemType; }

    /// Number of elements in this array
    size_t count() const { return _count; }

    /// Shorthand for `count() == DynamicCount`
    bool isDynamic() const { return count() == DynamicCount; }

    /// Shorthand for `!isDynamic()`
    bool isStatic() const { return !isDynamic(); }

    /// Recomputes size and align based on the element type and count. Used by
    /// `instantiateEntities()` to recompute the size for array types that have
    /// been instantiated before their element type is instantiated. This okay
    /// to be public because is has no effect if the size is already set
    /// correctly.
    void recomputeSize();

private:
    friend class Type;

    size_t alignImpl() const { return elemType->align(); }

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
class SCATHA_API PointerType: public BuiltinType, public PtrRefTypeBase {
protected:
    explicit PointerType(EntityType entityType, QualType base,
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
};

/// Represents a reference type
class SCATHA_API ReferenceType: public Type, public PtrRefTypeBase {
public:
    explicit ReferenceType(QualType base);

private:
    friend class Type;
    size_t sizeImpl() const { return 0; }
    size_t alignImpl() const { return 0; }
};

/// # OverloadSet

/// Groups a set of functions to perform overload resolution. Overload sets are
/// formed when a function is called and consist of all functions that are found
/// by name lookup at the call site.
/// Note that `OverloadSet`s are elusive entities. They are not placed within
/// the entity hierarchy and they have no name so they cannot be found by name
/// lookup. One overload set exists for every identifier that denotes a function
/// name and holds all functions that are visible at that point.
class SCATHA_API OverloadSet:
    public Entity,
    private utl::small_vector<Function*> {
public:
    explicit OverloadSet(SourceRange loc,
                         utl::small_vector<Function*> functions);

    OverloadSet(OverloadSet const&) = delete;

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
    explicit Generic(std::string name, size_t, Scope* parentScope):
        Entity(EntityType::Generic, std::move(name), parentScope) {}

    size_t numParameters() const { return numParams; }

private:
    size_t numParams;
};

/// Represents a different name for another entity
class SCATHA_API Alias: public Entity {
public:
    explicit Alias(std::string name, Entity& aliased, Scope* parent,
                   ast::ASTNode* astNode, AccessControl accessControl);

    /// \Returns the entity that this alias refers to
    Entity* aliased() { return _aliased; }

    /// \overload
    Entity const* aliased() const { return _aliased; }

private:
    friend class Entity;
    EntityCategory categoryImpl() const { return aliased()->category(); }

    Entity* _aliased;
};

///
class SCATHA_API TypeDeductionQualifier: public Entity {
public:
    explicit TypeDeductionQualifier(ReferenceKind refKind,
                                    Mutability mutability,
                                    PointerBindMode bindMode):
        Entity(EntityType::TypeDeductionQualifier,
               /* name= */ {},
               /* parent = */ nullptr,
               /* astNode = */ nullptr),
        _refKind(refKind),
        _mut(mutability),
        _bindMode(bindMode) {}

    ///
    ReferenceKind refKind() const { return _refKind; }

    ///
    Mutability mutability() const { return _mut; }

    ///
    PointerBindMode bindMode() const { return _bindMode; }

    /// \Returns `mutability() == Mutability::Mutable`
    bool isMutable() const { return mutability() == Mutability::Mutable; }

    /// \Returns `mutability() == Mutability::Const`
    bool isConst() const { return mutability() == Mutability::Const; }

private:
    friend class Entity;
    EntityCategory categoryImpl() const { return EntityCategory::Type; }

    ReferenceKind _refKind;
    Mutability _mut;
    PointerBindMode _bindMode;
};

/// # Poison

/// Represents a poison entity, an invalid entity to help spread redundant error
/// messages
class SCATHA_API PoisonEntity: public Entity {
public:
    explicit PoisonEntity(ast::Identifier* ID, EntityCategory cat,
                          Scope* parentScope, AccessControl accessControl);

private:
    friend class Entity;
    EntityCategory categoryImpl() const { return cat; }

    EntityCategory cat;
};

struct StripAliasFn {
    Entity* operator()(Entity* entity) const {
        return const_cast<Entity*>((*this)(static_cast<Entity const*>(entity)));
    }

    Entity const* operator()(Entity const* entity) const {
        if (auto* alias = dyncast<Alias const*>(entity)) {
            return alias->aliased();
        }
        return entity;
    }

    Entity& operator()(Entity& entity) const {
        return const_cast<Entity&>((*this)(static_cast<Entity const&>(entity)));
    }

    Entity const& operator()(Entity const& entity) const {
        return *(*this)(&entity);
    }
};

/// \Returns the aliased entity if \p entity is an alias, otherwise returns \p
/// entity
inline constexpr StripAliasFn stripAlias{};

} // namespace scatha::sema

#undef SC_ASTNODE_DERIVED

#endif // SCATHA_SEMA_ENTITY_H_
