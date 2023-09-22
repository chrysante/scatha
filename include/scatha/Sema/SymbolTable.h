#ifndef SCATHA_SEMA_SYMBOLTABLE_H_
#define SCATHA_SEMA_SYMBOLTABLE_H_

#include <memory>
#include <span>
#include <string>
#include <vector>

#include <utl/scope_guard.hpp>

#include <scatha/AST/Fwd.h>
#include <scatha/Common/Base.h>
#include <scatha/Common/Expected.h>
#include <scatha/Sema/Fwd.h>
#include <scatha/Sema/QualType.h>

namespace scatha {
class IssueHandler;
}

namespace scatha::sema {

class FunctionSignature;

class SCATHA_API SymbolTable {
public:
    static constexpr size_t InvalidSize = static_cast<size_t>(-1);

public:
    SymbolTable();
    SymbolTable(SymbolTable&&) noexcept;
    SymbolTable& operator=(SymbolTable&&) noexcept;
    ~SymbolTable();

    /// # Modifiers

    /// Declares a struct to the current scope without specifying size and
    /// alignment.
    ///
    /// For successful return the name must not have been declared
    /// before in the current scope.
    ///
    /// \returns a the declared type if no error occurs
    /// Otherwise emits an error to the issue handler
    StructType* declareStructureType(ast::StructDefinition* def);

    /// \overload for use without AST
    StructType* declareStructureType(std::string name);

    /// Declares a function name without signature to the current scope.
    ///
    /// For successful return the name must not have been declared
    /// before in the current scope as some entity other than `Function`
    ///
    /// \returns the declared function if no error occured
    /// Otherwise emits an error the to issue handler
    Function* declareFuncName(ast::FunctionDefinition* def);

    /// \overload for use without AST
    Function* declareFuncName(std::string name);

    /// Add signature to declared function.
    ///
    /// We need this two step way of addings functions to first scan
    /// all declarations to allow for forward references to other entities.
    ///
    /// \returns `true` if  \p signature is a legal overload
    /// Otherwise emits an error the to issue handler
    bool setFuncSig(Function* function, FunctionSignature signature);

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
    /// \returns the declared variable if no error occurs
    /// Otherwise emits an error the to issue handler
    Variable* declareVariable(ast::VarDeclBase* vardecl);

    /// \overload for use without AST
    Variable* declareVariable(std::string name);

    /// Declares a variable to the current scope.
    ///
    /// For successful return the name must not have been declared
    /// before in the current scope.
    ///
    /// \returns the declared variable if no error occurs
    /// Otherwise emits an error the to issue handler
    Variable* defineVariable(ast::VarDeclBase* vardecl,
                             Type const* type,
                             Mutability mutability);

    /// \overload for use without AST
    Variable* defineVariable(std::string name,
                             Type const* type,
                             Mutability mutability);

    ///
    ///
    ///
    Property* addProperty(PropertyKind kind, Type const* type);

    /// Creates a new unique temporary object of type \p type
    Temporary* temporary(QualType type);

    /// Declares an anonymous scope within the current scope.
    Scope* addAnonymousScope();

    /// Declares a poison entity to the current scope.
    void declarePoison(std::string name, EntityCategory category);

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
    decltype(auto) withScopeCurrent(Scope* scope, std::invocable auto&& f) {
        auto* stashed = &currentScope();
        utl::scope_guard guard([&] { makeScopeCurrent(stashed); });
        makeScopeCurrent(scope);
        return std::invoke(f);
    }

    /// Invoke function \p f with scope \p scope pushed
    /// This is essentially the same as `withScopeCurrent()` but it traps if \p
    /// scope is not a direct child of the current scope
    decltype(auto) withScopePushed(Scope* scope, std::invocable auto&& f) {
        utl::scope_guard guard([this] { popScope(); });
        pushScope(scope);
        return std::invoke(f);
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

    /// Set the issue handler for this symbol table.
    /// Setting the issue handler is necessary for making declarations.
    void setIssueHandler(IssueHandler& issueHandler);

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

    StructType* declareStructImpl(ast::StructDefinition* def, std::string name);
    Function* declareFuncImpl(ast::FunctionDefinition* def, std::string name);
    Variable* declareVarImpl(ast::VarDeclBase* vardecl, std::string name);
    Variable* defineVarImpl(ast::VarDeclBase* vardecl,
                            std::string name,
                            Type const* type,
                            Mutability mut);

    template <typename T, typename... Args>
    T* declareBuiltinType(Args&&... args);

    std::unique_ptr<Impl> impl;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_SYMBOLTABLE_H_
