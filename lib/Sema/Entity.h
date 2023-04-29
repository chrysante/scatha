#ifndef SCATHA_SEMA_ENTITY_H_
#define SCATHA_SEMA_ENTITY_H_

#include <concepts>
#include <string>
#include <string_view>
#include <utility>

#include <utl/hashmap.hpp>

#include <scatha/Common/Base.h>
#include <scatha/Sema/FunctionSignature.h>
#include <scatha/Sema/Fwd.h>
#include <scatha/Sema/SymbolID.h>

namespace scatha::sema {

/// # Base

/// Base class for all semantic entities in the language.
class SCATHA_API Entity {
public:
    struct MapHash: std::hash<SymbolID> {
        struct is_transparent;
        using std::hash<SymbolID>::operator();
        size_t operator()(Entity const& e) const {
            return std::hash<SymbolID>{}(e.symbolID());
        }
    };

    struct MapEqual {
        struct is_transparent;
        bool operator()(Entity const& a, Entity const& b) const {
            return a.symbolID() == b.symbolID();
        }
        bool operator()(Entity const& a, SymbolID b) const {
            return a.symbolID() == b;
        }
        bool operator()(SymbolID a, Entity const& b) const {
            return a == b.symbolID();
        }
    };

public:
    /// The name of this entity
    std::string_view name() const { return _name; }

    /// Mangled name of this entity
    std::string const& mangledName() const;

    /// The SymbolID of this entity
    SymbolID symbolID() const { return _symbolID; }

    /// `true` if this entity is unnamed
    bool isAnonymous() const { return _name.empty(); }

    /// The parent scope of this entity
    Scope* parent() { return _parent; }

    /// \overload
    Scope const* parent() const { return _parent; }

    /// The runtime type of this entity class
    EntityType entityType() const { return _entityType; }

protected:
    explicit Entity(EntityType entityType,
                    std::string name,
                    SymbolID symbolID,
                    Scope* parent):
        _entityType(entityType),
        _name(name),
        _symbolID(symbolID),
        _parent(parent) {}

private:
    EntityType _entityType;
    std::string _name;
    mutable std::string _mangledName;
    SymbolID _symbolID;
    Scope* _parent = nullptr;
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
                      SymbolID symbolID,
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

    // Until we have heterogenous lookup
    SymbolID findID(std::string_view name) const;

    bool isChildScope(SymbolID id) const { return _children.contains(id); }

    auto children() const;
    auto symbols() const;

protected:
    explicit Scope(EntityType entityType,
                   ScopeKind,
                   std::string name,
                   SymbolID symbolID,
                   Scope* parent);

private:
    friend class internal::ScopePrinter;
    friend class SymbolTable;
    void add(Entity* entity);

private:
    /// Scopes don't own their childscopes. These objects are owned by the
    /// symbol table.
    utl::hashmap<SymbolID, Scope*> _children;
    utl::hashmap<std::string, SymbolID> _symbols;
    ScopeKind _kind;
};

/// Represents an anonymous scope
class AnonymousScope: public Scope {
public:
    explicit AnonymousScope(SymbolID id, ScopeKind scopeKind, Scope* parent);
};

/// Represents the global scope
class GlobalScope: public Scope {
public:
    explicit GlobalScope(SymbolID id);
};

inline auto Scope::children() const {
    struct Iterator {
        Iterator begin() const {
            Iterator result = *this;
            result._itr     = _map.begin();
            return result;
        }
        Iterator end() const {
            Iterator result = *this;
            result._itr     = _map.end();
            return result;
        }
        Scope const& operator*() const { return *_itr->second; }
        Iterator& operator++() {
            ++_itr;
            return *this;
        }
        bool operator==(Iterator const& rhs) const { return _itr == rhs._itr; }
        using Map = utl::hashmap<SymbolID, Scope*>;
        Map const& _map;
        Map::const_iterator _itr;
    };
    return Iterator{ _children, {} };
}

inline auto Scope::symbols() const {
    struct Iterator {
        Iterator begin() const {
            Iterator result = *this;
            result._itr     = _map.begin();
            return result;
        }
        Iterator end() const {
            Iterator result = *this;
            result._itr     = _map.end();
            return result;
        }
        SymbolID operator*() const { return _itr->second; }
        Iterator& operator++() {
            ++_itr;
            return *this;
        }
        bool operator==(Iterator const& rhs) const { return _itr == rhs._itr; }
        using Map = utl::hashmap<std::string, SymbolID>;
        Map const& _map;
        Map::const_iterator _itr;
    };
    return Iterator{ _symbols, {} };
}

/// # Function

class SCATHA_API Function: public Scope {
public:
    explicit Function(std::string name,
                      SymbolID functionID,
                      SymbolID overloadSetID,
                      Scope* parentScope,
                      FunctionAttribute attrs):
        Scope(EntityType::Function,
              ScopeKind::Function,
              std::move(name),
              functionID,
              parentScope),
        _overloadSetID(overloadSetID),
        attrs(attrs) {}

    /// \Returns The type ID of this function.
    Type const* type() const { return signature().type(); }

    /// \Returns The overload set ID of this function.
    SymbolID overloadSetID() const { return _overloadSetID; }

    /// \Returns The signature of this function.
    FunctionSignature const& signature() const { return _sig; }

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

    void setAccessSpecifier(AccessSpecifier spec) { accessSpec = spec; }

    /// Set attribute \p attr to `true`.
    void setAttribute(FunctionAttribute attr) { attrs |= attr; }

    /// Set attribute \p attr to `false`.
    void removeAttribute(FunctionAttribute attr) { attrs &= ~attr; }

private:
    friend class SymbolTable; /// To set `_sig` and `_isExtern`
    FunctionSignature _sig;
    SymbolID _overloadSetID;
    FunctionAttribute attrs;
    AccessSpecifier accessSpec = AccessSpecifier::Private;
    u32 _slot      : 31        = 0;
    bool _isExtern : 1         = false;
    u32 _index                 = 0;
};

namespace internal {

struct FunctionArgumentsHash {
    struct is_transparent;
    size_t operator()(Function const* f) const {
        return f->signature().argumentHash();
    }
    size_t operator()(std::span<QualType const* const> const& args) const {
        return FunctionSignature::hashArguments(args);
    }
};

struct FunctionArgumentsEqual {
    struct is_transparent;

    using Args = std::span<QualType const* const>;

    bool operator()(Function const* a, Function const* b) const {
        return a->signature().argumentHash() == b->signature().argumentHash();
    }
    bool operator()(Function const* a, Args b) const {
        return a->signature().argumentHash() ==
               FunctionSignature::hashArguments(b);
    }
    bool operator()(Args a, Function const* b) const { return (*this)(b, a); }
    bool operator()(Args a, Args b) const {
        return FunctionSignature::hashArguments(a) ==
               FunctionSignature::hashArguments(b);
    }
};

} // namespace internal

/// # Types

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
    explicit OverloadSet(std::string name, SymbolID id, Scope* parentScope):
        Entity(EntityType::OverloadSet, std::move(name), id, parentScope) {}

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
    utl::hashset<Function*,
                 internal::FunctionArgumentsHash,
                 internal::FunctionArgumentsEqual>
        funcSet;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_ENTITY_H_
