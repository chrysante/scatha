#ifndef SCATHA_COMMON_IDENTIFIERTABLE_H_
#define SCATHA_COMMON_IDENTIFIERTABLE_H_

#include <memory>
#include <string>
#include <span>

#include "Common/Name.h"
#include "Common/Scope.h"
#include "Common/Type.h"
#include "Common/Variable.h"
#include "Common/Function.h"
#include "Common/ElementTable.h"


namespace scatha {
	
	class IdentifierTable {
	public:
		IdentifierTable();
		
		void pushScope(std::string name);
		void popScope();
		
		TypeEx& addType(std::string const& name);
		Function& addFunction(std::string const& name,
							  TypeID returnType,
							  std::span<TypeID const> argumentTypes);
		Variable& addVariable(std::string const& name, TypeID type);
		
		NameID lookupName(std::string const&) const;
		
		TypeEx& getType(NameID id) { return utl::as_mutable(utl::as_const(*this).getType(id)); }
		TypeEx const& getType(NameID) const;
		Function& getFunction(NameID id) { return utl::as_mutable(utl::as_const(*this).getFunction(id)); }
		Function const& getFunction(NameID) const;
		Variable& getVariable(NameID id) { return utl::as_mutable(utl::as_const(*this).getVariable(id)); }
		Variable const& getVariable(NameID) const;
		
	private:
		NameID addName(std::string const&, NameCategory);
		
	private:
		Scope* currentScope = nullptr;
		std::unique_ptr<Scope> globalScope;
		ElementTable<TypeEx> types;
		ElementTable<Function> funcs;
		ElementTable<Variable> vars;
	};
	
}

#endif // SCATHA_COMMON_IDENTIFIERTABLE_H_

