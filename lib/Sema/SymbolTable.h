// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_SEMA_SYMBOLTABLE_H_
#define SCATHA_SEMA_SYMBOLTABLE_H_

#include <atomic>
#include <memory>
#include <string>

#include <range/v3/view.hpp>
#include <utl/common.hpp>
#include <utl/hashmap.hpp>
#include <utl/hashset.hpp>
#include <utl/scope_guard.hpp>
#include <utl/vector.hpp>

#include <scatha/Common/Base.h>
#include <scatha/Common/Expected.h>
#include <scatha/Common/UniquePtr.h>
#include <scatha/Sema/Function.h>
#include <scatha/Sema/OverloadSet.h>
#include <scatha/Sema/QualType.h>
#include <scatha/Sema/Scope.h>
#include <scatha/Sema/SymbolID.h>
#include <scatha/Sema/Type.h>
#include <scatha/Sema/Variable.h>

namespace scatha::sema {

class SemanticIssue;

class SCATHA_API SymbolTable {
public:
    static constexpr size_t invalidSize = static_cast<size_t>(-1);

public:
    SymbolTable();
    SymbolTable(SymbolTable&&) noexcept;
    SymbolTable& operator=(SymbolTable&&) noexcept;
    ~SymbolTable();

    /// MARK: Modifiers

    /// \brief Declares an object type to the current scope without size and
    /// alignment.
    ///
    /// \details For successful return the name must not have been declared
    /// before in the current scope.
    ///
    /// \returns Reference to declared type if no error occurs.
    ///
    /// \returns `InvalidDeclaration` with reason `Redefinition` if declared
    /// name is already in use in the current scope.
    Expected<ObjectType&, SemanticIssue*> declareObjectType(
        std::string name, bool allowKeywords = false);

    /// Simpler interface to declare builtins. Internally calls
    /// `declareObjectType()`
    ///
    /// TODO: Only used internally. Make this private.
    Type const* declareBuiltinType(std::string name, size_t size, size_t align);

    /// \brief Declares a function to the current scope without signature.
    ///
    /// \details For successful return the name must not have been declared
    /// before in the current scope as some entity other than `Function`
    ///
    /// \returns Const reference to declared function if no error occurs.
    ///
    /// \returns `InvalidDeclaration` with reason `Redefinition` if declared
    /// name is already used by another kind of entity in the current scope.
    Expected<Function const&, SemanticIssue*> declareFunction(std::string name);

    /// \brief Add signature to declared function.
    ///
    /// \details We need this two step way of addings functions to first scan
    /// all declarations to allow for forward references to other entities.
    /// \returns Nothing if  \p signature is a legal overload.
    ///
    /// \returns `InvalidDeclaration` with reason `Redefinition` if \p signature
    /// is not a legal overload, with reason `CantOverloadOnReturnType` if \p
    /// signature has same arguments as another function in the overload set but
    /// different return type.
    Expected<void, SemanticIssue*> setSignature(SymbolID functionID,
                                                FunctionSignature signature);

    /// \brief Declares an external function.
    ///
    /// \details The name will be declared in the global scope, if it hasn't
    /// been declared before.

    /// \returns `true` iff declaration was successfull.
    ///
    bool declareExternalFunction(std::string name,
                                 size_t slot,
                                 size_t index,
                                 FunctionSignature signature,
                                 FunctionAttribute attrs);

    /// \brief Declares a variable to the current scope without type.
    ///
    /// \details For successful return the name must not have been declared
    /// before in the current scope.
    ///
    /// \returns Reference to declared variable if no error occurs.
    ///
    /// \returns `InvalidDeclaration` with reason `Redefinition` if declared
    /// name is already in use in the current scope.
    Expected<Variable&, SemanticIssue*> declareVariable(std::string name);

    /// \brief Declares a variable to the current scope.
    ///
    /// \details For successful return the name must not have been declared
    /// before in the current scope.
    ///
    /// \returns Reference to declared variable if no error occurs.
    ///
    /// \returns `InvalidDeclaration` with reason `Redefinition` if name of \p
    /// varDecl is already in use in the current scope.
    Expected<Variable&, SemanticIssue*> addVariable(std::string name,
                                                    QualType const* type,
                                                    size_t offset = 0);

    /// \brief Declares an anonymous scope within the current scope.
    ///
    /// \returns Reference to the new scope.
    Scope const& addAnonymousScope();

    QualType const* qualify(Type const* base,
                            TypeQualifiers qualifiers = TypeQualifiers::None,
                            size_t arraySize          = 0) const;

    QualType const* addQualifiers(QualType const* base,
                                  TypeQualifiers qualifiers) const {
        SC_ASSERT(base, "");
        return qualify(base, base->qualifiers() | qualifiers);
    }

    QualType const* removeQualifiers(QualType const* base,
                                     TypeQualifiers qualifiers) const {
        return qualify(base, base->qualifiers() & ~qualifiers);
    }

    /// Make type \p base into an array view of element type \p base
    QualType const* arrayView(
        Type const* base,
        TypeQualifiers qualifiers = TypeQualifiers::None) const {
        SC_ASSERT(base, "");
        return qualify(base,
                       TypeQualifiers::Array |
                           TypeQualifiers::ImplicitReference | qualifiers);
    }

    /// \brief Makes scope with symbolD \p id the current scope.
    ///
    /// \details \p id must reference a scope within the current scope.
    void pushScope(SymbolID id);

    /// \brief Makes current the parent scope of the current scope.
    ///
    /// \warning Current scope must not be the global scope.
    void popScope();

    /// \brief Makes \p scope the current scope.
    ///
    /// \details If \p scope is `nullptr` the global scope will be current after
    /// the call.
    void makeScopeCurrent(Scope* scope);

    decltype(auto) withScopePushed(SymbolID scopeID,
                                   std::invocable auto&& f) const {
        utl::scope_guard pop = [this] { utl::as_mutable(*this).popScope(); };
        utl::as_mutable(*this).pushScope(scopeID);
        return f();
    }

    decltype(auto) withScopeCurrent(Scope* scope,
                                    std::invocable auto&& f) const {
        SC_ASSERT(scope != nullptr, "");
        utl::scope_guard pop = [this, old = &utl::as_mutable(currentScope())] {
            utl::as_mutable(*this).makeScopeCurrent(old);
        };
        utl::as_mutable(*this).makeScopeCurrent(scope);
        return f();
    }

    /// MARK: Queries

    /// Access the entity corresponding to ID \p id
    /// Traps if \p id is invalid
    Entity& get(SymbolID id) {
        return const_cast<Entity&>(utl::as_const(*this).get(id));
    }

    /// \overload
    Entity const& get(SymbolID id) const;

    /// Access the entity corresponding to ID \p id as entity type `E`
    template <std::derived_from<Entity> E>
    E const& get(SymbolID id) const {
        return cast<E const&>(get(id));
    }

    /// \overload
    template <std::derived_from<Entity> E>
    E& get(SymbolID id) {
        return cast<E&>(get(id));
    }

    /// Access the entity corresponding to ID \p id
    /// \Returns Pointer to the entity or `nullptr` if \p id is invalid
    Entity const* tryGet(SymbolID id) const;

    /// \overload
    Entity* tryGet(SymbolID id) {
        return const_cast<Entity*>(utl::as_const(*this).tryGet(id));
    }

    /// Access the entity corresponding to ID \p id as entity type `E`
    /// \Returns Pointer to the entity or `nullptr` if \p id is invalid
    template <std::derived_from<Entity> E>
    E const* tryGet(SymbolID id) const {
        auto* entity = tryGet(id);
        return entity ? dyncast<E const*>(entity) : nullptr;
    }

    /// \overload
    template <std::derived_from<Entity> E>
    E* tryGet(SymbolID id) {
        return const_cast<E*>(utl::as_const(*this).tryGet<E>(id));
    }

    SymbolID builtinFunction(size_t index) const {
        return _builtinFunctions[index];
    }

    SymbolID lookup(std::string_view name) const;

    template <std::derived_from<Entity> E>
    E const* lookup(std::string_view name) const {
        return tryGet<E>(lookup(name));
    }

    Scope& currentScope() { return *_currentScope; }
    Scope const& currentScope() const { return *_currentScope; }

    Scope& globalScope() { return *_globalScope; }
    Scope const& globalScope() const { return *_globalScope; }

    /// Getters for builtin types
    Type const* Void() const { return _void; }
    Type const* Byte() const { return _byte; }
    Type const* Bool() const { return _bool; }
    Type const* Int() const { return _int; }
    Type const* Float() const { return _float; }
    Type const* String() const { return _string; }

    QualType const* qualVoid(
        TypeQualifiers qualifiers = TypeQualifiers::None) const {
        return qualify(_void, qualifiers);
    }

    QualType const* qualByte(
        TypeQualifiers qualifiers = TypeQualifiers::None) const {
        return qualify(_bool, qualifiers);
    }

    QualType const* qualBool(
        TypeQualifiers qualifiers = TypeQualifiers::None) const {
        return qualify(_bool, qualifiers);
    }

    QualType const* qualInt(
        TypeQualifiers qualifiers = TypeQualifiers::None) const {
        return qualify(_int, qualifiers);
    }

    QualType const* qualFloat(
        TypeQualifiers qualifiers = TypeQualifiers::None) const {
        return qualify(_float, qualifiers);
    }

    QualType const* qualString(
        TypeQualifiers qualifiers = TypeQualifiers::None) const {
        return qualify(_string, qualifiers);
    }

    /// Review if we want to keep these:
    void setSortedObjectTypes(utl::vector<TypeID> ids) {
        _sortedObjectTypes = std::move(ids);
    }

    /// View over topologically sorted object types
    std::span<TypeID const> sortedObjectTypes() const {
        return _sortedObjectTypes;
    }

    /// View over all functions
    std::span<Function const* const> functions() const { return _functions; }

private:
    QualType const* getQualType(ObjectType const* base,
                                TypeQualifiers qualifiers,
                                size_t arraySize) const;

    SymbolID generateID(SymbolCategory category) const;

private:
    GlobalScope* _globalScope = nullptr;
    Scope* _currentScope      = nullptr;

    mutable u64 _idCounter = 1;

    mutable utl::hashmap<SymbolID, UniquePtr<Entity>> _entities;

    mutable utl::hashmap<std::tuple<ObjectType const*, TypeQualifiers, size_t>,
                         QualType const*>
        _qualTypes;

    utl::vector<TypeID> _sortedObjectTypes;

    utl::small_vector<Function*> _functions;

    utl::vector<SymbolID> _builtinFunctions;

    /// Builtin types
    Type const *_void, *_byte, *_bool, *_int, *_float, *_string;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_SYMBOLTABLE_H_
