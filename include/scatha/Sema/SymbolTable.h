#ifndef SCATHA_SEMA_SYMBOLTABLE_H_
#define SCATHA_SEMA_SYMBOLTABLE_H_

#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include <utl/scope_guard.hpp>
#include <utl/vector.hpp>

#include <scatha/AST/Fwd.h>
#include <scatha/Common/Base.h>
#include <scatha/Common/Expected.h>
#include <scatha/Common/SourceLocation.h>
#include <scatha/Sema/Fwd.h>
#include <scatha/Sema/QualType.h>

namespace scatha {
class IssueHandler;
}

namespace scatha::sema {

/// Container of all entities in the program.
/// This als performs semantic checks on declarations such as redefinitions.
class SCATHA_API SymbolTable {
public:
    /// This is public so static functions in the implementation can name it
    struct Impl;

    SymbolTable();
    SymbolTable(SymbolTable&&) noexcept;
    SymbolTable& operator=(SymbolTable&&) noexcept;
    ~SymbolTable();

    /// # Modifiers

    ///
    FileScope* declareFileScope(std::string filename);

    /// Imports the library denoted by \p ID and declares an alias to it in the
    /// current scope
    NativeLibrary* makeNativeLibAvailable(ast::Identifier& ID);

    /// Imports the library denoted by \p name if not yet imported and declares
    /// it as hidden in the global scope.
    NativeLibrary* importNativeLib(std::string_view name);

    /// Imports a foreign library from the string literal \p lit. Searches the
    /// specified search paths for a shared library \pre \p lit must be a string
    /// literal
    ForeignLibrary* importForeignLib(ast::Literal& lit);

    /// Imports a foreign library from \p name. Searches the specified search
    /// paths for a shared library
    ForeignLibrary* importForeignLib(std::string_view name);

    /// Declares a struct to the current scope without specifying size and
    /// alignment.
    ///
    /// For successful return the name must not have been declared
    /// before in the current scope.
    ///
    /// \returns a the declared type if no error occurs
    /// Otherwise emits an error to the issue handler
    StructType* declareStructureType(ast::StructDefinition* def,
                                     AccessControl accessControl);

    /// \overload for use without AST
    StructType* declareStructureType(std::string name,
                                     AccessControl accessControl);

    /// Declares a function name without signature to the current scope.
    ///
    /// For successful return the name must not have been declared
    /// before in the current scope as some entity other than `Function`
    ///
    /// \returns the declared function if no error occured
    /// Otherwise emits an error the to issue handler
    Function* declareFuncName(ast::FunctionDefinition* def,
                              AccessControl accessControl);

    /// Add signature to declared function.
    ///
    /// We need this two step way of addings functions to first scan
    /// all declarations to allow for forward references to other entities.
    ///
    /// \returns `true` if  \p signature is a legal overload
    /// Otherwise emits an error the to issue handler
    bool setFunctionType(Function* function, FunctionType const* type);

    /// \overload that construct the function type
    bool setFunctionType(Function* function,
                         std::span<Type const* const> argumentTypes,
                         Type const* returnType);

    /// \overload for use without AST. Here we don't require two step
    /// initialization.
    Function* declareFunction(std::string name,
                              FunctionType const* type,
                              AccessControl accessControl);

    /// Add an overload set to the symbol table. This actually just exists so
    /// the symbol table owns the overload set so we have a stable address. See
    /// documentation of `OverloadSet`
    OverloadSet* addOverloadSet(SourceRange sourceRange,
                                utl::small_vector<Function*> functions);

    /// Declares an external function.
    ///
    /// The name will be declared in the global scope, if it hasn't
    /// been declared before.
    ///
    /// \returns the declared function or null when an error occurred
    Function* declareForeignFunction(std::string name,
                                     FunctionType const* type,
                                     FunctionAttribute attrs,
                                     AccessControl accessControl);

    /// Declares a variable to the current scope without type.
    ///
    /// For successful return the name must not have been declared
    /// before in the current scope.
    ///
    /// \returns the declared variable if no error occurs
    /// Otherwise emits an error the to issue handler
    Variable* declareVariable(ast::VarDeclBase* vardecl,
                              AccessControl accessControl);

    /// Two step variable definition for globally visible variables (including
    /// struct members)
    bool setVariableType(Variable* var, Type const* type);

    /// Declares a variable to the current scope.
    ///
    /// For successful return the name must not have been declared
    /// before in the current scope.
    ///
    /// \returns the declared variable if no error occurs
    /// Otherwise emits an error the to issue handler
    Variable* defineVariable(ast::VarDeclBase* vardecl,
                             Type const* type,
                             Mutability mutability,
                             AccessControl accessControl);

    /// \overload for use without AST
    Variable* defineVariable(std::string name,
                             Type const* type,
                             Mutability mutability,
                             AccessControl accessControl);

    ///
    Property* addProperty(PropertyKind kind,
                          Type const* type,
                          Mutability mut,
                          ValueCategory valueCat,
                          AccessControl accessControl);

    /// Creates a new unique temporary object of type \p type
    Temporary* temporary(QualType type);

    /// Declares an anonymous scope within the current scope.
    Scope* addAnonymousScope();

    /// Declares an alias to entity \p aliased under the name \p name in the
    /// current scope.
    /// Does nothing if \p aliased is already aliased under the same name or
    /// exists with the same name in the current scope
    Alias* declareAlias(std::string name,
                        Entity& aliased,
                        ast::ASTNode* astNode,
                        AccessControl accessControl);

    /// Declares an alias to entity \p aliased under the same name in the
    /// current scope.
    /// Does nothing if \p aliased is already aliased under the same name in the
    /// current scope or if \p aliased is a member of the current scope
    Alias* declareAlias(Entity& aliased,
                        ast::ASTNode* astNode,
                        AccessControl accessControl);

    /// Declares a poison entity to the current scope.
    PoisonEntity* declarePoison(ast::Identifier* ID,
                                EntityCategory category,
                                AccessControl accessControl);

    /// Makes scope \p scope the current scope.
    ///
    /// \p scope must be a child scope of the current scope.
    void pushScope(Scope* scope);

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
    decltype(auto) withScopeCurrent(Scope const* cscope,
                                    std::invocable auto&& f) const {
        auto* scope = const_cast<Scope*>(cscope);
        auto* self = const_cast<SymbolTable*>(this);
        auto* stashed = &self->currentScope();
        utl::scope_guard guard([=] { self->makeScopeCurrent(stashed); });
        self->makeScopeCurrent(scope);
        return std::invoke(f);
    }

    /// Invoke function \p f with scope \p scope pushed
    /// This is essentially the same as `withScopeCurrent()` but it traps if \p
    /// scope is not a direct child of the current scope
    decltype(auto) withScopePushed(Scope const* cscope,
                                   std::invocable auto&& f) const {
        auto* scope = const_cast<Scope*>(cscope);
        auto* self = const_cast<SymbolTable*>(this);
        utl::scope_guard guard([self] { self->popScope(); });
        self->pushScope(scope);
        return std::invoke(f);
    }

    /// # Accessors

    /// \Returns the `FunctionType` with argument types \p argumentTypes and
    /// return type \p returnType
    FunctionType const* functionType(std::span<Type const* const> argumentTypes,
                                     Type const* returnType);

    /// \overload
    FunctionType const* functionType(
        std::initializer_list<Type const*> argumentTypes,
        Type const* returnType);

    /// \Returns the `ArrayType` with element type \p elementType and \p size
    /// elements
    ArrayType const* arrayType(ObjectType const* elementType, size_t size);

    /// \overload
    /// \Returns the `ArrayType` with element type \p elementType and dynamic
    /// count
    ArrayType const* arrayType(ObjectType const* elementType);

    /// \Returns the `IntType` with size \p size and signedness \p signedness
    IntType const* intType(size_t width, Signedness signedness);

    /// \Returns the `RawPtrType` to the pointee type \p pointee
    RawPtrType const* pointer(QualType pointee);

    /// \Returns the `ReferenceType` to the referred type \p referred
    ReferenceType const* reference(QualType referred);

    /// \Returns the `UniquePtrType` to the pointee type \p pointee
    UniquePtrType const* uniquePointer(QualType pointee);

    /// ## Queries

    /// \Returns a view over all functions
    std::span<Function* const> functions();

    /// \overload
    std::span<Function const* const> functions() const;

    /// \Returns a view over all struct types
    std::span<StructType const* const> structTypes() const;

    /// View over all imported libraries
    std::span<Library* const> importedLibs();

    /// \overload
    std::span<Library const* const> importedLibs() const;

    /// \Returns a list of resolved foreign library paths
    std::vector<std::filesystem::path> foreignLibraryPaths() const;

    /// \Returns a list of the names of imported foreign libraries
    std::vector<std::string> foreignLibraryNames() const;

    /// All entities
    std::vector<Entity const*> entities() const;

    /// Find entities with name \p name starting in the current scope and
    /// subsequently searching all parent scopes. If the first found entity is a
    /// function, an overload set will be build from the function in the
    /// currently searched scope and the parent scopes. If the first found
    /// entity is not a function, all entities from the currently searched scope
    /// will be returned If \p findHiddenEntities is true, also invisible
    /// entities will be found
    utl::small_vector<Entity*> unqualifiedLookup(
        std::string_view name, bool findHiddenEntities = false);

    /// Set the issue handler for this symbol table.
    /// Setting the issue handler is necessary for making declarations.
    void setIssueHandler(IssueHandler& issueHandler);

    ///
    void setLibrarySearchPaths(std::span<std::filesystem::path const> paths);

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

    /// Getters for builtin types
    /// @{
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
    NullPtrType const* NullPtr() const;

    IntType const* Int() const { return S64(); }
    FloatType const* Float() const { return F32(); }
    FloatType const* Double() const { return F64(); }
    /// @}

private:
    NativeLibrary* getOrImportNativeLib(std::string_view name,
                                        ast::ASTNode* astNode);
    ForeignLibrary* getOrImportForeignLib(std::string_view name,
                                          ast::ASTNode* astNode);

    StructType* declareStructImpl(ast::StructDefinition* def,
                                  std::string name,
                                  AccessControl accCtrl);
    Function* declareFuncImpl(ast::FunctionDefinition* def,
                              std::string name,
                              AccessControl accCtrl);
    Variable* declareVarImpl(ast::VarDeclBase* vardecl,
                             std::string name,
                             AccessControl accCtrl,
                             Mutability mut);
    Variable* defineVarImpl(ast::VarDeclBase* vardecl,
                            std::string name,
                            Type const* type,
                            Mutability mut,
                            AccessControl accCtrl);

    template <typename T, typename... Args>
    T* declareBuiltinType(Args&&... args);

    /// Adds \p entity as a child of the current scope
    void addToCurrentScope(Entity* entity);

    /// Declares an alias to \p entity under the same name in the global scope
    /// if \p entity is declared public in a filescope
    Alias* addGlobalAliasIfInternalAtFilescope(Entity* entity);

    /// \Pre expects \p entity to have access control
    bool validateAccessControl(Entity const& entity);

    bool checkRedef(int kind,
                    std::string_view name,
                    ast::Declaration const* declaration,
                    AccessControl accessControl);

    std::unique_ptr<Impl> impl;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_SYMBOLTABLE_H_
