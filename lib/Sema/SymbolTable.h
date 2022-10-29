#ifndef SCATHA_SEMA_SYMBOLTABLE2_H_
#define SCATHA_SEMA_SYMBOLTABLE2_H_

#include <atomic>
#include <memory>
#include <string>

#include <utl/common.hpp>
#include <utl/hashmap.hpp>
#include <utl/hashset.hpp>
#include <utl/scope_guard.hpp>

#include "AST/AST.h"
#include "Basic/Basic.h"
#include "Common/Expected.h"
#include "Common/Token.h"
#include "Sema/ObjectType.h"
#include "Sema/OverloadSet.h"
#include "Sema/Scope.h"
#include "Sema/SemanticIssue.h"
#include "Sema/Variable.h"

namespace scatha::sema {

class SCATHA(API) SymbolTable {
public:
    static constexpr size_t invalidSize = static_cast<size_t>(-1);

public:
    SymbolTable();

    /// MARK: Modifiers

    /// \brief Declares an object type to the current scope without size and alignment.
    ///
    /// \details For successful return the name must not have been declared before in the current scope.
    ///
    /// \returns Reference to declared type if no error occurs.
    ///
    /// \returns \p InvalidDeclaration with reason \p Redefinition if declared name is already in use in the current
    /// scope.
    Expected<ObjectType&, SemanticIssue> declareObjectType(ast::StructDefinition const& structDef);

    /// \overload
    Expected<ObjectType&, SemanticIssue> declareObjectType(Token name);

    /// Simpler interface to declare builtins. Internally calls \p declareObjectType()
    TypeID declareBuiltinType(std::string name, size_t size, size_t align);

    /// \brief Declares a function to the current scope without signature.
    ///
    /// \details For successful return the name must not have been declared before in the current scope as some entity
    /// other than \p Function \returns Const reference to declared function if no error occurs.
    ///
    /// \returns \p
    /// InvalidDeclaration with reason \p Redefinition if declared name is already used by another kind of entity in the
    /// current scope.
    Expected<Function const&, SemanticIssue> declareFunction(ast::FunctionDefinition const& functionDef);

    /// \overload
    Expected<Function const&, SemanticIssue> declareFunction(Token name);

    /// \brief Add signature to declared function.
    ///
    /// \details We need this two step way of addings functions to first scan all declarations to allow for forward
    /// references to other entities. \returns Nothing if  \p signature is a legal overload.
    ///
    /// \returns \p InvalidDeclaration with reason \p Redefinition if \p signature is not a legal overload, with reason
    /// \p CantOverloadOnReturnType if \p signature has same arguments as another function in the overload set but
    /// different return type.
    Expected<void, SemanticIssue> setSignature(SymbolID functionID, FunctionSignature signature);

    /// \brief Declares a variable to the current scope without type.
    ///
    /// \details For successful return the name must not have been declared before in the current scope.
    ///
    /// \returns Reference to declared variable if no error occurs.
    ///
    /// \returns \p InvalidDeclaration with reason \p Redefinition if declared name is already in use in the current
    /// scope.
    Expected<Variable&, SemanticIssue> declareVariable(ast::VariableDeclaration const& varDecl);

    /// \overload
    Expected<Variable&, SemanticIssue> declareVariable(Token name);

    /// \brief Declares a variable to the current scope.
    ///
    /// \details For successful return the name must not have been declared before in the current scope.
    ///
    /// \returns Reference to declared variable if no error occurs.
    ///
    /// \returns \p InvalidDeclaration with reason \p Redefinition if \p name is already in use in the current scope.
    Expected<Variable&, SemanticIssue> addVariable(ast::VariableDeclaration const& varDecl, TypeID, size_t offset = 0);

    /// \overload
    Expected<Variable&, SemanticIssue> addVariable(Token name, TypeID, size_t offset = 0);

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
    /// \details If \p scope is NULL the global scope will be current after the call.
    void makeScopeCurrent(Scope* scope);

    decltype(auto) withScopePushed(SymbolID scopeID, std::invocable auto&& f) const {
        utl::scope_guard pop = [this] { utl::as_mutable(*this).popScope(); };
        utl::as_mutable(*this).pushScope(scopeID);
        return f();
    }

    decltype(auto) withScopeCurrent(Scope* scope, std::invocable auto&& f) const {
        SC_ASSERT(scope != nullptr, "");
        utl::scope_guard pop = [this, old = &utl::as_mutable(currentScope())] {
            utl::as_mutable(*this).makeScopeCurrent(old);
        };
        utl::as_mutable(*this).makeScopeCurrent(scope);
        return f();
    }

    /// MARK: Queries

    OverloadSet const& getOverloadSet(SymbolID) const;
    OverloadSet& getOverloadSet(SymbolID id) { return utl::as_mutable(utl::as_const(*this).getOverloadSet(id)); }
    OverloadSet const* tryGetOverloadSet(SymbolID) const;
    OverloadSet* tryGetOverloadSet(SymbolID id) {
        return const_cast<OverloadSet*>(utl::as_const(*this).tryGetOverloadSet(id));
    }

    Function const& getFunction(SymbolID) const;
    Function& getFunction(SymbolID id) { return utl::as_mutable(utl::as_const(*this).getFunction(id)); }
    Function const* tryGetFunction(SymbolID) const;
    Function* tryGetFunction(SymbolID id) { return const_cast<Function*>(utl::as_const(*this).tryGetFunction(id)); }

    Variable const& getVariable(SymbolID) const;
    Variable& getVariable(SymbolID id) { return utl::as_mutable(utl::as_const(*this).getVariable(id)); }
    Variable const* tryGetVariable(SymbolID) const;
    Variable* tryGetVariable(SymbolID id) { return const_cast<Variable*>(utl::as_const(*this).tryGetVariable(id)); }

    ObjectType const& getObjectType(SymbolID) const;
    ObjectType& getObjectType(SymbolID id) { return utl::as_mutable(utl::as_const(*this).getObjectType(id)); }
    ObjectType const* tryGetObjectType(SymbolID) const;
    ObjectType* tryGetObjectType(SymbolID id) {
        return const_cast<ObjectType*>(utl::as_const(*this).tryGetObjectType(id));
    }

    std::string getName(SymbolID id) const;

    SymbolID lookup(std::string_view name) const;
    SymbolID lookup(Token const& token) const { return lookup(token.id); }

    OverloadSet const* lookupOverloadSet(std::string_view name) const;
    OverloadSet const* lookupOverloadSet(Token const& token) const { return lookupOverloadSet(token.id); }
    Variable const* lookupVariable(std::string_view name) const;
    Variable const* lookupVariable(Token const& token) const { return lookupVariable(token.id); }
    ObjectType const* lookupObjectType(std::string_view name) const;
    ObjectType const* lookupObjectType(Token const& token) const { return lookupObjectType(token.id); }

    bool is(SymbolID, SymbolCategory) const;

    SymbolCategory categorize(SymbolID) const;

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

private:
    SymbolID generateID();

private:
    std::unique_ptr<GlobalScope> _globalScope;
    Scope* _currentScope = nullptr;

    u64 _idCounter = 1;

    /// Must be \p node_hashset
    /// References to the elements are stored by the surrounding scopes.
    template <typename T>
    using EntitySet = utl::node_hashset<T, EntityBase::MapHash, EntityBase::MapEqual>;

    EntitySet<OverloadSet> _overloadSets;
    EntitySet<Function> _functions;
    EntitySet<Variable> _variables;
    EntitySet<ObjectType> _objectTypes;
    EntitySet<FunctionSignature> _signatures;
    EntitySet<Scope> _anonymousScopes;

    /// Builtin types
    TypeID _void, _bool, _int, _float, _string;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_SYMBOLTABLE2_H_
