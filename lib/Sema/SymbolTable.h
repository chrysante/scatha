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

    /// \brief Declares a poison entity to the current scope.
    ///
    /// \returns `InvalidDeclaration` with reason `Redefinition` if declared
    /// name is already in use in the current scope.
    Expected<PoisonEntity&, SemanticIssue*> declarePoison(std::string name);

    ///
    ///
    ArrayType const* arrayType(ObjectType const* elementType, size_t size);

    /// \Returns The `QualType` with base type \p base and reference qualifier
    /// \p ref
    QualType const* qualify(ObjectType const* base,
                            Reference ref  = Reference::None,
                            Mutability mut = Mutability::Mutable);

    /// \Returns The `QualType` with same base type as \p type but without any
    /// qualifications
    QualType const* stripQualifiers(QualType const* type);

    /// \Returns The `QualType` with same qualifiers as \p type but with
    /// \p objType as base
    QualType const* copyQualifiers(QualType const* from, ObjectType const* to);

    /// \Returns The `QualType` with same base type as \p type but reference
    /// qualification \p ref
    QualType const* setReference(QualType const* type, Reference ref);

    /// \Returns The `QualType` with same base type as \p type but mutability
    /// qualification \p mut
    QualType const* setMutable(QualType const* type, Mutability mut);

    /// Make type \p base into a reference to dynamic array of element type \p
    /// base
    QualType const* qDynArray(ObjectType const* base,
                              Reference ref,
                              Mutability mut = Mutability::Mutable);

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
        return dyncast_or_null<E const*>(lookup(name));
    }

    Scope& currentScope() { return *_currentScope; }
    Scope const& currentScope() const { return *_currentScope; }

    GlobalScope& globalScope() { return *_globalScope; }
    GlobalScope const& globalScope() const { return *_globalScope; }

    /// Getters for builtin types
    VoidType const* Void() const { return _void; }
    ByteType const* Byte() const { return _byte; }
    BoolType const* Bool() const { return _bool; }
    IntType const* S8() const { return _s8; }
    IntType const* S16() const { return _s16; }
    IntType const* S32() const { return _s32; }
    IntType const* S64() const { return _s64; }
    IntType const* U8() const { return _u8; }
    IntType const* U16() const { return _u16; }
    IntType const* U32() const { return _u32; }
    IntType const* U64() const { return _u64; }
    FloatType const* F64() const { return _f64; }

    IntType const* intType(size_t width, Signedness signedness);

    QualType const* qVoid(Reference = Reference::None);

    QualType const* qByte(Reference = Reference::None);

    QualType const* qBool(Reference = Reference::None);

    QualType const* qS8(Reference = Reference::None);

    QualType const* qS16(Reference = Reference::None);

    QualType const* qS32(Reference = Reference::None);

    QualType const* qS64(Reference = Reference::None);

    QualType const* qU8(Reference = Reference::None);

    QualType const* qU16(Reference = Reference::None);

    QualType const* qU32(Reference = Reference::None);

    QualType const* qU64(Reference = Reference::None);

    QualType const* qF64(Reference = Reference::None);

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
    template <typename T, typename... Args>
    T* declareBuiltinType(Args&&... args);

    template <typename E, typename... Args>
    E* addEntity(Args&&... args);

private:
    GlobalScope* _globalScope = nullptr;
    Scope* _currentScope      = nullptr;

    u64 _idCounter = 1;

    utl::vector<UniquePtr<Entity>> _entities;

    utl::hashmap<std::tuple<ObjectType const*, Reference, Mutability>,
                 QualType const*>
        _qualTypes;

    utl::hashmap<std::pair<ObjectType const*, size_t>, ArrayType const*>
        _arrayTypes;

    utl::vector<StructureType*> _sortedStructureTypes;

    utl::small_vector<Function*> _functions;

    utl::vector<Function*> _builtinFunctions;

    /// Builtin types
    VoidType* _void;
    ByteType* _byte;
    BoolType* _bool;
    IntType* _s8;
    IntType* _s16;
    IntType* _s32;
    IntType* _s64;
    IntType* _u8;
    IntType* _u16;
    IntType* _u32;
    IntType* _u64;
    FloatType* _f64;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_SYMBOLTABLE_H_
