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
#include <scatha/Sema/Fwd.h>

namespace scatha::sema {

class FunctionSignature;
class SemanticIssue;

class SCATHA_API SymbolTable {
public:
    static constexpr size_t InvalidSize = static_cast<size_t>(-1);

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
    Expected<StructureType&, SemanticIssue*> declareStructureType(
        std::string name, bool allowKeywords = false);

    /// Simpler interface to declare builtins. Internally calls
    /// `declareStructureType()`
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
    Expected<Function&, SemanticIssue*> declareFunction(std::string name);

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
    Expected<void, SemanticIssue*> setSignature(Function* function,
                                                FunctionSignature signature);

    /// \brief Declares an external function.
    ///
    /// \details The name will be declared in the global scope, if it hasn't
    /// been declared before.

    /// \returns `true` iff declaration was successfull.
    ///
    bool declareSpecialFunction(FunctionKind kind,
                                std::string name,
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
    Scope& addAnonymousScope();

    ///
    ///
    ArrayType const* arrayType(ObjectType const* elementType, size_t size);

    ///
    QualType const* qualify(Type const* base,
                            TypeQualifiers qualifiers = TypeQualifiers::None);

    ///
    QualType const* addQualifiers(QualType const* base,
                                  TypeQualifiers qualifiers);

    ///
    QualType const* removeQualifiers(QualType const* base,
                                     TypeQualifiers qualifiers);

    /// Make type \p base into an array view of element type \p base
    QualType const* arrayView(ObjectType const* base,
                              TypeQualifiers qualifiers = TypeQualifiers::None);

    /// \brief Makes scope \p scope the current scope.
    ///
    /// \details \p scope must be a child scope of the current scope.
    void pushScope(Scope* scope);

    /// \brief Makes current the parent scope of the current scope.
    ///
    /// \warning Current scope must not be the global scope.
    void popScope();

    /// \brief Makes \p scope the current scope.
    ///
    /// \details If \p scope is `nullptr` the global scope will be current after
    /// the call.
    void makeScopeCurrent(Scope* scope);

    decltype(auto) withScopePushed(Scope* scope,
                                   std::invocable auto&& f) const {
        utl::scope_guard pop = [this] { utl::as_mutable(*this).popScope(); };
        utl::as_mutable(*this).pushScope(scope);
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

    /// ## Queries

    Function* builtinFunction(size_t index) const {
        return _builtinFunctions[index];
    }

    /// Find entity by name within the current scope
    Entity* lookup(std::string_view name) {
        return const_cast<Entity*>(std::as_const(*this).lookup(name));
    }

    /// \overload
    Entity const* lookup(std::string_view name) const;

    /// Lookup entity and `dyncast` to type `E`
    template <std::derived_from<Entity> E>
    E* lookup(std::string_view name) {
        return const_cast<E*>(std::as_const(*this).lookup<E>(name));
    }

    /// \overload
    template <std::derived_from<Entity> E>
    E const* lookup(std::string_view name) const {
        Entity const* result = lookup(name);
        return result ? dyncast<E const*>(result) : nullptr;
    }

    Scope& currentScope() { return *_currentScope; }
    Scope const& currentScope() const { return *_currentScope; }

    GlobalScope& globalScope() { return *_globalScope; }
    GlobalScope const& globalScope() const { return *_globalScope; }

    /// Getters for builtin types
    StructureType const* Void() const { return _void; }
    StructureType const* Byte() const { return _byte; }
    StructureType const* Bool() const { return _bool; }
    StructureType const* S64() const { return _s64; }
    StructureType const* Float() const { return _float; }
    StructureType const* String() const { return _string; }

    QualType const* qVoid(TypeQualifiers qualifiers = TypeQualifiers::None);

    QualType const* qByte(TypeQualifiers qualifiers = TypeQualifiers::None);

    QualType const* qBool(TypeQualifiers qualifiers = TypeQualifiers::None);

    QualType const* qS64(TypeQualifiers qualifiers = TypeQualifiers::None);

    QualType const* qFloat(TypeQualifiers qualifiers = TypeQualifiers::None);

    QualType const* qString(TypeQualifiers qualifiers = TypeQualifiers::None);

    /// Review if we want to keep these:
    void setSortedStructureTypes(utl::vector<StructureType*> ids) {
        _sortedStructureTypes = std::move(ids);
    }

    /// View over topologically sorted object types
    std::span<StructureType const* const> sortedStructureTypes() const {
        return _sortedStructureTypes;
    }

    /// View over all functions
    std::span<Function const* const> functions() const { return _functions; }

private:
    QualType const* getQualType(ObjectType const* base,
                                TypeQualifiers qualifiers);

    template <typename E, typename... Args>
    E* addEntity(Args&&... args);

private:
    GlobalScope* _globalScope = nullptr;
    Scope* _currentScope      = nullptr;

    u64 _idCounter = 1;

    utl::vector<UniquePtr<Entity>> _entities;

    utl::hashmap<std::pair<ObjectType const*, TypeQualifiers>, QualType const*>
        _qualTypes;

    utl::hashmap<std::pair<ObjectType const*, size_t>, ArrayType const*>
        _arrayTypes;

    utl::vector<StructureType*> _sortedStructureTypes;

    utl::small_vector<Function*> _functions;

    utl::vector<Function*> _builtinFunctions;

    /// Builtin types
    StructureType const *_void, *_byte, *_bool, *_s64, *_float, *_string;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_SYMBOLTABLE_H_
