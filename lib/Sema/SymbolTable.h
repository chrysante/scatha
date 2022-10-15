#ifndef SCATHA_SEMA_SYMBOLTABLE2_H_
#define SCATHA_SEMA_SYMBOLTABLE2_H_

#include <atomic>
#include <memory>
#include <string>

#include <utl/common.hpp>
#include <utl/hashmap.hpp>
#include <utl/scope_guard.hpp>

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
    SymbolTable();

    // Modifiers
    Expected<Function const&, SemanticIssue> addFunction(Token name, FunctionSignature);

    Expected<Variable&, SemanticIssue> addVariable(Token name, TypeID, size_t offset = 0);

    Expected<ObjectType&, SemanticIssue>
    addObjectType(Token name, size_t size = -1, size_t align = -1, bool isBuiltin = false);

    Scope const& addAnonymousScope();

    void pushScope(SymbolID id);

    void popScope();

    // scope == nullptr -> global scope will be current
    void makeScopeCurrent(Scope*);

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

    // Queries
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

    // Must be node_hashset! references to the elements are stored by the
    // surrounding scopes.
    template <typename T>
    using EntitySet = utl::node_hashset<T, EntityBase::MapHash, EntityBase::MapEqual>;

    EntitySet<OverloadSet> _overloadSets;
    EntitySet<Variable> _variables;
    EntitySet<ObjectType> _objectTypes;
    EntitySet<FunctionSignature> _signatures;
    EntitySet<Scope> _anonymousScopes;
    utl::hashmap<SymbolID, Function*> _functions;

    /// Builtin types
    TypeID _void, _bool, _int, _float, _string;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_SYMBOLTABLE2_H_
