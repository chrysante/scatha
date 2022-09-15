#include "SymbolTable.h"

namespace scatha::sem {
	
	SymbolTable::SymbolTable() {
		_globalScope = std::make_unique<Scope>(std::string{}, nullptr);
		_currentScope = _globalScope.get();
		
		_void   = defineType("void",    0, 0).id();
		_bool   = defineType("bool",    1, 1).id();
		_int    = defineType("int",     8, 8).id();
		_float  = defineType("float",   8, 8).id();
		_string = defineType("string", 24, 8).id();
	}
	
	std::pair<NameID, bool> SymbolTable::addName(std::string_view name, NameCategory cat) {
		auto const [nameID, newlyAdded] = _currentScope->addName(name, cat);
		if (!newlyAdded && nameID.category() != cat) {
			throw ScopeError(_currentScope, name, cat, nameID.category(), ScopeError::NameCategoryConflict);
		}
		return { nameID, newlyAdded };
	}
	
	void SymbolTable::pushScope(std::string_view name) {
		auto const id = _currentScope->findIDByName(name);
		pushScope(id);
	}
	
	void SymbolTable::pushScope(NameID id) {
		_currentScope = _currentScope->childScope(id);
	}
	
	void SymbolTable::popScope() {
		_currentScope = _currentScope->parentScope();
		SC_ASSERT(_currentScope != nullptr, "Already in global scope");
	}
	
	NameID SymbolTable::declareType(std::string_view name) {
		auto const [id, _] = addName(name, NameCategory::Type);
		return id;
	}

	TypeEx& SymbolTable::defineType(std::string_view name, size_t size, size_t align) {
		auto const [id, newlyAdded] = addName(name, NameCategory::Type);
		
		auto [type, success] = types.emplace(id.id(), std::string(name), TypeID(id.id()), size, align);
		if (!success) {
			throw ScopeError(_currentScope, name, ScopeError::NameAlreadyExists);
		}
		SC_ASSERT_AUDIT(type != nullptr, "");
		return *type;
	}
	
	std::pair<Function*, bool> SymbolTable::declareFunction(std::string_view name, TypeID returnType, std::span<TypeID const> argumentTypes) {
		auto const [nameID, newlyAdded] = addName(name, NameCategory::Function);
		
		auto const computedFunctionTypeID = computeFunctionTypeID(returnType, argumentTypes);
		
		if (newlyAdded) {
			// Since function types are not named, we use the TypeID as the key here. This should be fine, since function types cannot be found by name anyhow. So wen can just find them by their hashed signature aka their TypeID.
			[[maybe_unused]] auto [functionType, _] = types.emplace((u64)computedFunctionTypeID, returnType, argumentTypes, computedFunctionTypeID);
			auto const result = funcs.emplace(nameID.id(), nameID, computedFunctionTypeID);
			SC_ASSERT(result.second, "Function already exists");
			return result;
		}
		
		// Now the function has already been declared. Verify that the type matches.
		
		if (nameID.category() != NameCategory::Function) {
			throw;
		}
		auto& function = funcs.get(nameID.id());
		
		if (function.typeID() != computedFunctionTypeID) {
			throw;
		}
		
		auto const& functionType = types.get((u64)computedFunctionTypeID);
		functionTypeVerifyEqual(functionType, returnType, argumentTypes);
		
		return { &function, false };
	}
	
	
	std::pair<Variable*, bool> SymbolTable::declareVariable(std::string_view name, TypeID typeID, bool isConstant) {
		auto const [nameID, newlyAdded] = addName(name, NameCategory::Variable);
		return vars.emplace(nameID.id(), nameID, typeID);
	}

	NameID SymbolTable::lookupName(std::string_view name, NameCategory category) const {
		std::optional<NameID> result;
		Scope const* sc = _currentScope;
		while (true) {
			result = sc->tryFindIDByName(name);
			if (result) {
				if (category != NameCategory::None && !test(category & result->category())) {
					/// TODO: Throw something else here
					throw ScopeError(sc, name, category, result->category(), ScopeError::NameCategoryConflict);
				}
				return *result;
			}
			sc = sc->parentScope();
			if (sc == nullptr) {
				throw ScopeError(_currentScope, name, ScopeError::NameNotFound);
			}
		}
	}
	
	TypeEx const& SymbolTable::findTypeByName(std::string_view name) const {
		NameID const id = lookupName(name);
		if (id.category() != NameCategory::Type) {
			throw; // throw some proper exception here
		}
		return getType(id);
	}
	
	TypeEx const& SymbolTable::getType(TypeID id) const {
		return types.get((u64)id);
	}
	
	Function const& SymbolTable::getFunction(NameID id) const {
		return funcs.get(id.id());
	}
	
	Variable const& SymbolTable::getVariable(NameID id) const {
		return vars.get(id.id());
	}
	
}
