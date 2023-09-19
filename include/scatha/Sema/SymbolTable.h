#ifndef SCATHA_SEMA_SYMBOLTABLE_H_
#define SCATHA_SEMA_SYMBOLTABLE_H_

#include <memory>
#include <span>
#include <string>
#include <vector>

#include <scatha/Common/Base.h>
#include <scatha/Common/Expected.h>
#include <scatha/Sema/Fwd.h>
#include <scatha/Sema/QualType.h>

namespace scatha::sema {

class FunctionSignature;
class SemanticIssue;

class SCATHA_API SymbolTable {
public:
    static constexpr size_t InvalidSize = static_cast<size_t>(-1);

public:
    SymbolTable();

    SymbolTable(SymbolTable const& rhs) = delete;

    SymbolTable& operator=(SymbolTable const& rhs) = delete;

    SymbolTable(SymbolTable&&) noexcept;

    SymbolTable& operator=(SymbolTable&&) noexcept;

    ~SymbolTable();

    /// # Modifiers

    /// Declares an object type to the current scope without size and
    /// alignment.
    ///
    /// For successful return the name must not have been declared
    /// before in the current scope.
    ///
    /// \returns Reference to declared type if no error occurs or
    /// `InvalidDeclaration` with reason `Redefinition` if declared
    /// name is already in use in the current scope.
    Expected<StructType&, SemanticIssue*> declareStructureType(
        std::string name);

    /// Declares a function to the current scope without signature.
    ///
    /// For successful return the name must not have been declared
    /// before in the current scope as some entity other than `Function`
    ///
    /// \returns Const reference to declared function if no error occurs or
    /// `InvalidDeclaration` with reason `Redefinition` if declared
    /// name is already used by another kind of entity in the current scope.
    Expected<Function&, SemanticIssue*> declareFunction(std::string name);

    /// Add signature to declared function.
    ///
    /// We need this two step way of addings functions to first scan
    /// all declarations to allow for forward references to other entities.
    ///
    /// \returns Nothing if  \p signature is a legal overload or
    /// `InvalidDeclaration` with reason `Redefinition` if \p signature
    /// is not a legal overload, with reason `CantOverloadOnReturnType` if \p
    /// signature has same arguments as another function in the overload set but
    /// different return type.
    Expected<void, SemanticIssue*> setSignature(Function* function,
                                                FunctionSignature signature);

    /// Declares an external function.
    ///
    /// The name will be declared in the global scope, if it hasn't
    /// been declared before.
    ///
    /// \returns `true` if declaration was successful.
    bool declareSpecialFunction(FunctionKind kind,
                                std::string name,
                                size_t slot,
                                size_t index,
                                FunctionSignature signature,
                                FunctionAttribute attrs);

    /// Declares a variable to the current scope without type.
    ///
    /// For successful return the name must not have been declared
    /// before in the current scope.
    ///
    /// \returns Reference to declared variable if no error occurs or
    /// `InvalidDeclaration` with reason `Redefinition` if declared
    /// name is already in use in the current scope.
    Expected<Variable&, SemanticIssue*> declareVariable(std::string name);

    /// Declares a variable to the current scope.
    ///
    /// For successful return the name must not have been declared
    /// before in the current scope.
    ///
    /// \returns Reference to declared variable if no error occurs or
    /// `InvalidDeclaration` with reason `Redefinition` if name of
    /// \p varDecl is already in use in the current scope.
    Expected<Variable&, SemanticIssue*> addVariable(std::string name,
                                                    QualType type);

    ///
    ///
    ///
    Property& addProperty(PropertyKind kind, QualType type);

    /// Creates a new unique temporary object of type \p type
    Temporary* temporary(QualType type);

    /// Declares an anonymous scope within the current scope.
    Scope& addAnonymousScope();

    /// Declares a poison entity to the current scope.
    ///
    /// \returns `InvalidDeclaration` with reason `Redefinition` if declared
    /// name is already in use in the current scope.
    Expected<PoisonEntity&, SemanticIssue*> declarePoison(
        std::string name, EntityCategory category);

    /// Makes scope \p scope the current scope.
    ///
    /// \p scope must be a child scope of the current scope.
    void pushScope(Scope* scope);

    /// Makes scope with name \p name the current scope.
    ///
    /// \Returns `false` if \p name is not the same of a child scope of the
    /// current scope, `true` otherwise
    bool pushScope(std::string_view name);

    /// Makes current the parent scope of the current scope.
    ///
    /// \warning Current scope must not be the global scope.
    void popScope();

    /// Makes \p scope the current scope.
    ///
    /// If \p scope is `nullptr` the global scope will be current after
    /// the call.
    void makeScopeCurrent(Scope* scope);

    /// Invoke function \p f with scope \p scope made current
    decltype(auto) withScopeCurrent(Scope* scope,
                                    std::invocable auto&& f) const {
        static constexpr bool IsVoid =
            std::is_same_v<std::invoke_result_t<decltype(f)>, void>;
        auto& mutThis = const_cast<SymbolTable&>(*this);
        auto* old = &const_cast<Scope&>(currentScope());
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

    /// # Accessors

    /// \Returns the `ArrayType` with element type \p elementType and \p size
    /// elements
    ArrayType const* arrayType(ObjectType const* elementType, size_t size);

    /// \overload
    /// \Returns the `ArrayType` with element type \p elementType and dynamic
    /// count
    ArrayType const* arrayType(ObjectType const* elementType);

    /// \Returns the `IntType` with size \p size and signedness \p signedness
    IntType const* intType(size_t width, Signedness signedness);

    /// \Returns the `PointerType` to the pointee type \p pointee and
    /// reference qualifier \p ref
    PointerType const* pointer(QualType pointee);

    /// \Returns the `ReferenceType` to the referred type \p referred and
    /// reference qualifier \p ref
    ReferenceType const* reference(QualType referred);

    /// ## Queries

    /// View over all functions
    std::span<Function* const> functions();

    /// \overload
    std::span<Function const* const> functions() const;

    /// All entities
    std::vector<Entity const*> entities() const;

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

    ArrayType const* Str() const;

private:
    struct Impl;

    template <typename T, typename... Args>
    T* declareBuiltinType(Args&&... args);

    std::unique_ptr<Impl> impl;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_SYMBOLTABLE_H_
