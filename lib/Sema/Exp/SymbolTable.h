#ifndef SCATHA_SEMA_SYMBOLTABLE2_H_
#define SCATHA_SEMA_SYMBOLTABLE2_H_

#include <atomic>
#include <memory>
#include <string>

#include <utl/hashmap.hpp>

#include "Basic/Basic.h"
#include "Common/Expected.h"
#include "Sema/Exp/OverloadSet.h"
#include "Sema/Exp/Scope.h"
#include "Sema/Exp/Variable.h"
#include "Sema/Exp/ObjectType.h"
#include "Sema/Exp/SymbolIssue.h"

namespace scatha::sema::exp {
	
	class SCATHA(API) SymbolTable {
	public:
		SymbolTable();
		
		// Modifiers
		Expected<Function const&, OverloadIssue> addFunction(std::string name, FunctionSignature);
		Expected<Variable const&, SymbolCollisionIssue> addVariable(std::string name, TypeID);
		Expected<ObjectType&, SymbolCollisionIssue> addObjectType(std::string name, size_t size = -1, size_t align = -1);
		
		void pushScope(SymbolID id);
		
		void popScope();
		
		// Queries
		OverloadSet const& getOverloadSet(SymbolID) const;
		Variable const& getVariable(SymbolID) const;
		ObjectType const& getObjectType(SymbolID) const;
		
		SymbolID lookup(std::string_view name) const;
		
		OverloadSet const* lookupOverloadSet(std::string_view name) const;
		Variable const* lookupVariable(std::string_view name) const;
		ObjectType const* lookupObjectType(std::string_view name) const;
				
		Scope& currentScope() { return *_currentScope; }
		Scope const& currentScope() const { return *_currentScope; }
		
		/// Getters for builtin types
		TypeID Void()  const { return _void; }
		TypeID Bool()  const { return _bool; }
		TypeID Int()   const { return _int; }
		TypeID Float() const { return _float; }
		
	private:
		SymbolID generateID();
		
	private:
		std::unique_ptr<GlobalScope> _globalScope;
		Scope* _currentScope = nullptr;
		
		std::atomic<u64> _idCounter = 1;
		
		// Must be node_hashset! references to the elements are stored by the surrounding scopes.
		template <typename T>
		using EntitySet = utl::node_hashset<T, EntityBase::MapHash, EntityBase::MapEqual>;
		
		EntitySet<OverloadSet> _overloadSets;
		EntitySet<Variable> _variables;
		EntitySet<ObjectType> _objectTypes;
		EntitySet<FunctionSignature> _signatures;
		
		/// Builtin types
		TypeID _void, _bool, _int, _float;
	};
	
}

#endif // SCATHA_SEMA_SYMBOLTABLE2_H_

