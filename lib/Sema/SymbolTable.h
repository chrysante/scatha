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

#include <scatha/AST/Fwd.h> // Remove?
#include <scatha/Common/Base.h>
#include <scatha/Common/Expected.h>
#include <scatha/Sema/Function.h>
#include <scatha/Sema/ObjectType.h>
#include <scatha/Sema/OverloadSet.h>
#include <scatha/Sema/Scope.h>
#include <scatha/Sema/SymbolID.h>
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
    TypeID declareBuiltinType(std::string name, size_t size, size_t align);

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
                                                    TypeID,
                                                    size_t offset = 0);

    /// \brief Declares an anonymous scope within the current scope.
    ///
    /// \returns Reference to the new scope.
    Scope const& addAnonymousScope();

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

    OverloadSet const& getOverloadSet(SymbolID) const;
    OverloadSet& getOverloadSet(SymbolID id) {
        return utl::as_mutable(utl::as_const(*this).getOverloadSet(id));
    }
    OverloadSet const* tryGetOverloadSet(SymbolID) const;
    OverloadSet* tryGetOverloadSet(SymbolID id) {
        return const_cast<OverloadSet*>(
            utl::as_const(*this).tryGetOverloadSet(id));
    }

    Function const& getFunction(SymbolID) const;
    Function& getFunction(SymbolID id) {
        return utl::as_mutable(utl::as_const(*this).getFunction(id));
    }
    Function const* tryGetFunction(SymbolID) const;
    Function* tryGetFunction(SymbolID id) {
        return const_cast<Function*>(utl::as_const(*this).tryGetFunction(id));
    }

    Variable const& getVariable(SymbolID) const;
    Variable& getVariable(SymbolID id) {
        return utl::as_mutable(utl::as_const(*this).getVariable(id));
    }
    Variable const* tryGetVariable(SymbolID) const;
    Variable* tryGetVariable(SymbolID id) {
        return const_cast<Variable*>(utl::as_const(*this).tryGetVariable(id));
    }

    ObjectType const& getObjectType(SymbolID) const;
    ObjectType& getObjectType(SymbolID id) {
        return utl::as_mutable(utl::as_const(*this).getObjectType(id));
    }
    ObjectType const* tryGetObjectType(SymbolID) const;
    ObjectType* tryGetObjectType(SymbolID id) {
        return const_cast<ObjectType*>(
            utl::as_const(*this).tryGetObjectType(id));
    }

    std::string getName(SymbolID id) const;

    SymbolID builtinFunction(size_t index) const {
        return _builtinFunctions[index];
    }

    SymbolID lookup(std::string_view name) const;

    OverloadSet const* lookupOverloadSet(std::string_view name) const;
    Variable const* lookupVariable(std::string_view name) const;
    ObjectType const* lookupObjectType(std::string_view name) const;

    Scope& currentScope() { return *_currentScope; }
    Scope const& currentScope() const { return *_currentScope; }

    Scope& globalScope() { return *_globalScope; }
    Scope const& globalScope() const { return *_globalScope; }

    /// Getters for builtin types
    TypeID Void() const { return _void; }
    TypeID Bool() const { return _bool; }
    TypeID Int() const { return _int; }
    TypeID Float() const { return _float; }
    TypeID String() const { return _string; }

    /// Review if we want to keep these:
    void setSortedObjectTypes(utl::vector<TypeID> ids) {
        _sortedObjectTypes = std::move(ids);
    }
    std::span<TypeID const> sortedObjectTypes() const {
        return _sortedObjectTypes;
    }
    auto functions() const {
        return _functions | ranges::views::transform(
                                [](auto& p) -> auto& { return p.second; });
    }

private:
    SymbolID generateID(SymbolCategory category);

private:
    std::unique_ptr<GlobalScope> _globalScope;
    Scope* _currentScope = nullptr;

    u64 _idCounter = 1;

    /// Must be `node_hashmap`
    /// References to the elements are stored by the surrounding scopes.
    template <typename T>
    using EntityMap = utl::node_hashmap<SymbolID, T>;

    EntityMap<OverloadSet> _overloadSets;
    EntityMap<Function> _functions;
    EntityMap<Variable> _variables;
    EntityMap<ObjectType> _objectTypes;
    EntityMap<FunctionSignature> _signatures;
    EntityMap<Scope> _anonymousScopes;

    utl::vector<TypeID> _sortedObjectTypes;

    utl::vector<SymbolID> _builtinFunctions;

    /// Builtin types
    TypeID _void, _bool, _int, _float, _string;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_SYMBOLTABLE_H_
