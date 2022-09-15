#ifndef SCATHA_SEMANTICANALYZER_SYMBOLTABLE_H_
#define SCATHA_SEMANTICANALYZER_SYMBOLTABLE_H_

#include <memory>
#include <string>
#include <span>

#include "SemanticAnalyzer/SemanticElements.h"
#include "SemanticAnalyzer/Scope.h"

namespace scatha::sem {
	
	/**
	 SymbolTable:
	 Tree of all scopes and names in a translation unit
	 
	 # Notes: #
	 1. call addName() to add a new name to the current scope. Must be called before using pushScope with that name
	 */
	class SymbolTable {
	public:
		SymbolTable();
	
		/**
		 Add a name to the current scope.
		 
		 - parameter \p name: Name to add to the current scope.
		 - parameter \p category: Category of the name.
		 
		 - returns: Pair of ID of the added or existing name and boolean wether the name already existed.
		 
		 # Notes: #
		 1. \p name may already exist in the current scope. Then it is verified that \p category is the same as for the last call to this function.
		 2. This function will also be called internally by \p declare*define-Type*Function*Variable()
		 */
		std::pair<NameID, bool> addName(std::string_view name, NameCategory category);
		
		/**
		 Make current one of the child scopes of the current scope.
		 
		 - parameter \p name: Name of the scope to make current.
		 
		 - warning: \p name must be a child scope of the current scope.
		 */
		void pushScope(std::string_view name);
		void pushScope(NameID name);
		
		/**
		 Make current the parent scope of the current scope.
		 
		 - warning: Current scope must not be the global scope.
		 */
		void popScope();
		
		/**
		 Declare a type in the current scope.
		 
		 - parameter \p name: Name of the type.
		 
		 - returns: ID of the added or existing name.
		 
		 # Notes: #
		 1. \p name may already exist, however must be of category type.
		 So this function may be called multiple times with the same name.
		 */
		NameID declareType(std::string_view name);
		
		/**
		 Define a type in the current scope.
		 
		 - parameter \p name: Name of the type.
		 - parameter \p size: Size of the type in bytes.
		 - parameter \p align: Alignment of the type in bytes.
		 
		 - returns: Reference to the added type object.
		 
		 # Notes: #
		 1. \p name may already exist, however must not yet be defined and must be of category type.
		 So this function must not be called multiple times with the same name.
		 */
		TypeEx& defineType(std::string_view name, size_t size, size_t align);
		
		/**
		 Declare a function in the current scope.
		 
		 - parameter \p name: Name of the function.
		 - parameter \p returnType: TypeID of the returned type.
		 - parameter \p argumentTypes: TypeIDs of the arguments.
		 
		 - returns: Pair of pointer to the added or existing function and boolean wether the name already existed.
		 
		 # Notes: #
		 1. \p name may already exist, however must be of category function.
		 So this function may be called multiple times with the same name, however the signature must match.
		 */
		std::pair<Function*, bool> declareFunction(std::string_view name, TypeID returnType, std::span<TypeID const> argumentTypes);
		
		/**
		 Declare a variable in the current scope.
		 
		 - parameter \p name: Name of the variable.
		 - parameter \p typeID: TypeID of the variable.
		 - parameter \p isConstant: boolean indicating constness.
		 
		 - returns: Pair of pointer to the added or existing variable and boolean wether the name already existed.
		 
		 # Notes: #
		 1. \p name may already exist, however must be of category variable.
		 So this function may be called multiple times with the same name, however the type must match.
		 */
		std::pair<Variable*, bool> declareVariable(std::string_view name, TypeID typeID, bool isConstant);
		
		/**
		 Performs scoped name lookup. Looks for the name in the current scope and walks up the scope tree until it finds the name or reaches the global scope.
		 
		 - parameter \p name: Name to look for.
		 - parameter \p category: If \p category != \p None it verifies that the name is of the specified category.
		 
		 - returns: ID of the found name.
		 
		 # Notes: #
		 1. Throws \p ScopeError exception if the name cannot be found.
		 2. Throws \p ScopeError exception if the name is of the wring category.
		 */
		NameID lookupName(std::string_view name, NameCategory category = NameCategory::None) const;
		
		Scope const* currentScope() const { return _currentScope; }
		Scope const* globalScope() const { return _globalScope.get(); }
		
		TypeEx& findTypeByName(std::string_view name) {
			return utl::as_mutable(utl::as_const(*this).findTypeByName(name));
		}
		
		TypeEx const& findTypeByName(std::string_view name) const;
		
		TypeEx& getType(TypeID id) { return utl::as_mutable(utl::as_const(*this).getType(id)); }
		TypeEx const& getType(TypeID) const;
		TypeEx& getType(NameID id) { return getType(TypeID(id.id())); }
		TypeEx const& getType(NameID id) const { return getType(TypeID(id.id())); }
		
		Function& getFunction(NameID id) { return utl::as_mutable(utl::as_const(*this).getFunction(id)); }
		Function const& getFunction(NameID) const;
		
		Variable& getVariable(NameID id) { return utl::as_mutable(utl::as_const(*this).getVariable(id)); }
		Variable const& getVariable(NameID) const;
		
		TypeID Void() const  { return _void; }
		TypeID Bool() const  { return _bool; }
		TypeID Int() const   { return _int; }
		TypeID Float() const { return _float; }
		TypeID String() const { return _string; }
		
	private:
		
		
	private:
		Scope* _currentScope = nullptr;
		std::unique_ptr<Scope> _globalScope;
		ElementTable<TypeEx> types;
		ElementTable<Function> funcs;
		ElementTable<Variable> vars;
		
		/// Keyword types and values
		TypeID _void{}, _bool{}, _int{}, _float, _string{};
	};
	
}

#endif // SCATHA_SEMANTICANALYZER_SYMBOLTABLE_H_

