// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_SEMA_SYMBOLTABLE_H_
#define SCATHA_SEMA_SYMBOLTABLE_H_

#include <memory>
#include <span>
#include <string>
#include <vector>

#include <scatha/Common/Base.h>
#include <scatha/Common/Expected.h>
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
    /// \returns `InvalidDeclaration` with reason `Redefinition` if name of
    /// \p varDecl is already in use in the current scope.
    Expected<Variable&, SemanticIssue*> addVariable(std::string name,
                                                    QualType const* type);

    /// \brief Declares an anonymous scope within the current scope.
    ///
    /// \returns Reference to the new scope.
    Scope& addAnonymousScope();

    /// \brief Declares a poison entity to the current scope.
    ///
    /// \returns `InvalidDeclaration` with reason `Redefinition` if declared
    /// name is already in use in the current scope.
    Expected<PoisonEntity&, SemanticIssue*> declarePoison(
        std::string name, EntityCategory category);

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

    /// Invoke function \p f with scope \p scope made current
    decltype(auto) withScopeCurrent(Scope* scope,
                                    std::invocable auto&& f) const {
        static constexpr bool IsVoid =
            std::is_same_v<std::invoke_result_t<decltype(f)>, void>;
        auto& mutThis = const_cast<SymbolTable&>(*this);
        auto* old     = &const_cast<Scope&>(currentScope());
        mutThis.makeScopeCurrent(scope);
        if constexpr (IsVoid) {
            std::invoke(f);
            mutThis.makeScopeCurrent(old);
        }
        else {
            decltype(auto) result = std::invoke(f);
            mutThis.makeScopeCurrent(old);
            return result;
        }
    }

    /// ## Queries

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

    /// \Returns The builtin function at index \p index
    Function* builtinFunction(size_t index) const;

    /// \Returns The currently active scope
    Scope& currentScope();

    /// \overload
    Scope const& currentScope() const;

    /// \Returns The global scope
    GlobalScope& globalScope();

    /// \overload
    GlobalScope const& globalScope() const;

    /// ## Getters for builtin types

    VoidType const* Void() const;

    ByteType const* Byte() const;

    BoolType const* Bool() const;

    IntType const* S8() const;

    IntType const* S16() const;

    IntType const* S32() const;

    IntType const* S64() const;

    IntType const* U8() const;

    IntType const* U16() const;

    IntType const* U32() const;

    IntType const* U64() const;

    FloatType const* F32() const;

    FloatType const* F64() const;

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

    QualType const* qF32(Reference = Reference::None);

    QualType const* qF64(Reference = Reference::None);

    /// Review if we want to keep these:
    void setStructDependencyOrder(std::vector<StructureType*> ids);

    /// View over topologically sorted struct types
    std::span<StructureType const* const> structDependencyOrder() const;

    /// View over all functions
    std::span<Function const* const> functions() const;

private:
    struct Impl;

    template <typename T, typename... Args>
    T* declareBuiltinType(Args&&... args);

    template <typename E, typename... Args>
    E* addEntity(Args&&... args);

    std::unique_ptr<Impl> impl;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_SYMBOLTABLE_H_
