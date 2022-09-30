#ifndef SCATHA_SEMA_SYMBOLTABLE2_H_
#define SCATHA_SEMA_SYMBOLTABLE2_H_

#include <atomic>
#include <memory>
#include <string>

#include <utl/hashmap.hpp>

#include "Basic/Basic.h"
#include "Common/Expected.h"
#include "Common/Token.h"
#include "Sema/OverloadSet.h"
#include "Sema/Scope.h"
#include "Sema/Variable.h"
#include "Sema/ObjectType.h"
#include "Sema/SymbolIssue.h"
//#include "SymbolCategory.h"

namespace scatha::sema {
	
	class SCATHA(API) SymbolTable {
	public:
		SymbolTable();
		
		// Modifiers
		Expected<Function const&, SymbolIssue> addFunction(std::string name,
														   FunctionSignature);
		Expected<Function const&, SymbolIssue> addFunction(Token token,
														   FunctionSignature sig)
		{ return addFunction(std::move(token.id), std::move(sig)); }
		
		Expected<Variable const&, SymbolCollisionIssue> addVariable(std::string name,
																	TypeID,
																	bool isConstant);
		Expected<Variable const&, SymbolCollisionIssue> addVariable(Token token,
																	TypeID typeID,
																	bool isConstant)
		{ return addVariable(std::move(token.id), typeID, isConstant); }
		
		Expected<ObjectType&, SymbolCollisionIssue> addObjectType(std::string name,
																  size_t size = -1,
																  size_t align = -1,
																  bool isBuiltin = false);
		Expected<ObjectType&, SymbolCollisionIssue> addObjectType(Token token,
																  size_t size = -1,
																  size_t align = -1,
																  bool isBuiltin = false)
		{ return addObjectType(std::move(token.id), size, align, isBuiltin); }
		
		Scope const& addAnonymousScope();
		
		void pushScope(SymbolID id);
		
		void popScope();
		
		// Queries
		OverloadSet const& getOverloadSet(SymbolID) const;
		OverloadSet const* tryGetOverloadSet(SymbolID) const;
		Function const& getFunction(SymbolID) const;
		Function const* tryGetFunction(SymbolID) const;
		Variable const& getVariable(SymbolID) const;
		Variable const* tryGetVariable(SymbolID) const;
		ObjectType const& getObjectType(SymbolID) const;
		ObjectType const* tryGetObjectType(SymbolID) const;
		
		SymbolID lookup(std::string_view name) const;
		SymbolID lookup(Token const& token) const { return lookup(token.id); }
		
		OverloadSet const* lookupOverloadSet(std::string_view name) const;
		OverloadSet const* lookupOverloadSet(Token const& token) const { return lookupOverloadSet(token.id); }
		Variable const* lookupVariable(std::string_view name) const;
		Variable const* lookupVariable(Token const& token) const { return lookupVariable(token.id); }
		ObjectType const* lookupObjectType(std::string_view name) const;
		ObjectType const* lookupObjectType(Token const& token) const { return lookupObjectType(token.id); }
		
		bool is(SymbolID, SymbolCategory) const;
		
		Scope& currentScope() { return *_currentScope; }
		Scope const& currentScope() const { return *_currentScope; }
		
		Scope& globalScope() { return *_globalScope; }
		Scope const& globalScope() const { return *_globalScope; }
		
		/// Getters for builtin types
		TypeID Void()   const { return _void; }
		TypeID Bool()   const { return _bool; }
		TypeID Int()    const { return _int; }
		TypeID Float()  const { return _float; }
		TypeID String() const { return _string; }
		
	private:
		SymbolID generateID();
		
	private:
		std::unique_ptr<GlobalScope> _globalScope;
		Scope* _currentScope = nullptr;
		
		u64 _idCounter = 1;
		
		// Must be node_hashset! references to the elements are stored by the surrounding scopes.
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
	
}

#endif // SCATHA_SEMA_SYMBOLTABLE2_H_

