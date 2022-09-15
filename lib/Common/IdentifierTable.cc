#include "IdentifierTable.h"

#include "TypeTable.h"

namespace scatha {
	
	IdentifierTable::IdentifierTable() {
		globalScope = std::make_unique<Scope>(std::string{}, nullptr);
		currentScope = globalScope.get();
	}
	
	void IdentifierTable::pushScope(std::string name) {
		auto const id = currentScope->findIDByName(name);
		currentScope = currentScope->childScope(id);
	}
	
	void IdentifierTable::popScope() {
		currentScope = currentScope->parentScope();
		SC_ASSERT(currentScope != nullptr, "Already in global scope");
	}
	
	TypeEx& IdentifierTable::addType(std::string const& name) {
		auto const id = addName(name, NameCategory::Type);
		return types.emplace(id.id(), name, TypeID(id.id()), 0);
	}
	
	Function& IdentifierTable::addFunction(std::string const& name,
										   TypeID returnType,
										   std::span<TypeID const> argumentTypes)
	{
		auto const nameID = addName(name, NameCategory::Function);
		auto const typeID = computeFunctionTypeID(returnType, argumentTypes);
		types.emplace((u64)typeID, returnType, argumentTypes, typeID);
		return funcs.emplace(nameID.id(), typeID);
	}
	
	Variable& IdentifierTable::addVariable(std::string const& name, TypeID typeID) {
		auto const nameID = addName(name, NameCategory::Value);
		return vars.emplace(nameID.id(), typeID);
	}
	
	NameID IdentifierTable::lookupName(std::string const& name) const {
		std::optional<NameID> result;
		Scope const* sc = currentScope;
		while (true) {
			result = sc->tryFindIDByName(name);
			if (result) { return *result; }
			sc = sc->parentScope();
			if (sc == nullptr) {
				throw ScopeError(currentScope, name, ScopeError::NameNotFound);
			}
		}
	}
	
	TypeEx const& IdentifierTable::getType(NameID id) const {
		return types.get(id.id());
	}
	
	Function const& IdentifierTable::getFunction(NameID id) const {
		return funcs.get(id.id());
	}
	
	Variable const& IdentifierTable::getVariable(NameID id) const {
		return vars.get(id.id());
	}
	
	NameID IdentifierTable::addName(std::string const& name, NameCategory cat) {
		return currentScope->addName(name, cat);
	}
	
	
	
}
