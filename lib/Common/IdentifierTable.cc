#include "IdentifierTable.h"

#include "TypeTable.h"

namespace scatha {
	
	IdentifierTable::IdentifierTable() {
		globalScope = std::make_unique<Scope>(std::string{}, nullptr);
		currentScope = globalScope.get();
		
		auto& _void_t   = defineType("void");
		_void_t._size = 0;
		auto& _bool_t   = defineType("bool");
		_bool_t._size = 1;
		auto& _int_t    = defineType("int");
		_int_t._size = 8;
		auto& _float_t  = defineType("float");
		_float_t._size = 8;
		auto& _string_t = defineType("string");
		_string_t._size = sizeof(std::string);
		
		_void   = _void_t.id();
		_bool   = _bool_t.id();
		_int    = _int_t.id();
		_float  = _float_t.id();
		_string = _string_t.id();
	}
	
	std::pair<NameID, bool> IdentifierTable::addName(std::string const& name, NameCategory cat) {
		auto const [nameID, newlyAdded] = currentScope->addName(name, cat);
		if (!newlyAdded && nameID.category() != cat) {
			throw ScopeError(currentScope, name, cat, nameID.category(), ScopeError::NameCategoryConflict);
		}
		return { nameID, newlyAdded };
	}
	
	void IdentifierTable::pushScope(std::string name) {
		auto const id = currentScope->findIDByName(name);
		currentScope = currentScope->childScope(id);
	}
	
	void IdentifierTable::popScope() {
		currentScope = currentScope->parentScope();
		SC_ASSERT(currentScope != nullptr, "Already in global scope");
	}
	
	NameID IdentifierTable::declareType(std::string const& name) {
		auto const [id, _] = addName(name, NameCategory::Type);
		return id;
	}

	TypeEx& IdentifierTable::defineType(std::string const& name) {
		auto const [id, newlyAdded] = addName(name, NameCategory::Type);
		
		auto [type, success] = types.emplace(id.id(), name, TypeID(id.id()), 0);
		if (!success) {
			throw ScopeError(currentScope, name, ScopeError::NameAlreadyExists);
		}
		SC_ASSERT_AUDIT(type != nullptr, "");
		return *type;
	}
	
	std::pair<Function*, bool> IdentifierTable::declareFunction(std::string const& name) {
		auto const [nameID, newlyAdded] = addName(name, NameCategory::Function);
		return funcs.emplace(nameID.id(), nameID);
	}
	
	
	std::pair<Variable*, bool> IdentifierTable::declareVariable(std::string const& name, TypeID typeID, bool isConstant) {
		auto const [nameID, newlyAdded] = addName(name, NameCategory::Value);
		return vars.emplace(nameID.id(), nameID, typeID);
	}

	NameID IdentifierTable::lookupName(std::string const& name, NameCategory category) const {
		std::optional<NameID> result;
		Scope const* sc = currentScope;
		while (true) {
			result = sc->tryFindIDByName(name);
			if (result) {
				if (category != NameCategory::None && category != result->category()) {
					throw ScopeError(sc, name, category, result->category(), ScopeError::NameCategoryConflict);
				}
				return *result;
			}
			sc = sc->parentScope();
			if (sc == nullptr) {
				throw ScopeError(currentScope, name, ScopeError::NameNotFound);
			}
		}
	}
	
	TypeEx const& IdentifierTable::findTypeByName(std::string const& name) const {
		NameID const id = lookupName(name);
		if (id.category() != NameCategory::Type) {
			throw; // throw some proper exception here
		}
		return getType(id);
	}
	
	TypeEx const& IdentifierTable::getType(TypeID id) const {
		return types.get((u64)id);
	}
	
	Function const& IdentifierTable::getFunction(NameID id) const {
		return funcs.get(id.id());
	}
	
	Variable const& IdentifierTable::getVariable(NameID id) const {
		return vars.get(id.id());
	}
	
	
	
	
	
}
