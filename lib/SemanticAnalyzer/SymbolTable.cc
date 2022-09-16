#include "SymbolTable.h"

#include "SemanticAnalyzer/SemanticError.h"

namespace scatha::sem {
	
	SymbolTable::SymbolTable() {
		_globalScope = std::make_unique<Scope>(std::string{}, Scope::Global, nullptr);
		_currentScope = _globalScope.get();
		
		// TODO: Find a better solution for this
		_void   = defineType(Token("void"),    0, 0).id();
		_bool   = defineType(Token("bool"),    1, 1).id();
		_int    = defineType(Token("int"),     8, 8).id();
		_float  = defineType(Token("float"),   8, 8).id();
		_string = defineType(Token("string"), 24, 8).id();
	}
	
	std::pair<NameID, bool> SymbolTable::addName(Token const& name, NameCategory cat) {
		auto const [nameID, newlyAdded] = _currentScope->addName(name.id, cat);
		if (!newlyAdded && nameID.category() != cat) {
			throw InvalidRedeclaration(name, currentScope(), nameID.category());
		}
		return { nameID, newlyAdded };
	}
	
	void SymbolTable::pushScope(std::string_view name) {
		auto const id = _currentScope->findIDByName(name);
		SC_ASSERT(id, "Probably use of undeclared identifier, should have been handled before");
		pushScope(*id);
	}
	
	void SymbolTable::pushScope(NameID id) {
		_currentScope = _currentScope->childScope(id);
	}
	
	void SymbolTable::popScope() {
		_currentScope = _currentScope->parentScope();
		SC_ASSERT(_currentScope != nullptr, "Can't pop anymore as we are already in global scope");
	}
	
	NameID SymbolTable::declareType(Token const& name) {
		auto const [id, _] = addName(name, NameCategory::Type);
		return id;
	}

	TypeEx& SymbolTable::defineType(Token const& name, size_t size, size_t align) {
		auto const [id, newlyAdded] = addName(name, NameCategory::Type);
		
		auto [type, success] = types.emplace(id.id(), name.id, TypeID(id.id()), size, align);
		if (!success) {
/// MARK: Can't test this yet
			throw InvalidRedeclaration(name, currentScope());
		}
		SC_ASSERT_AUDIT(type != nullptr, "If the type has been added before it'd better not be null");
		return *type;
	}
	
	std::pair<Function*, bool> SymbolTable::declareFunction(Token const& name, TypeID returnType, std::span<TypeID const> argumentTypes) {
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
/// MARK: Can't test this yet
			throw InvalidRedeclaration(name, currentScope(), NameCategory::Function);
		}
		auto& function = funcs.get(nameID.id());
		auto const& functionType = types.get((u64)function.typeID());
		functionTypeVerifyEqual(name, functionType, returnType, argumentTypes);
		
		return { &function, false };
	}
	
	
	std::pair<Variable*, bool> SymbolTable::declareVariable(Token const& name, TypeID typeID, bool isConstant) {
		auto const [nameID, newlyAdded] = addName(name, NameCategory::Variable);
		return vars.emplace(nameID.id(), nameID, typeID, isConstant);
	}

	NameID SymbolTable::lookupName(Token const& name) const {
		std::optional<NameID> result;
		Scope const* sc = _currentScope;
		while (true) {
			result = sc->findIDByName(name.id);
			if (result) {
				return *result;
			}
			sc = sc->parentScope();
			if (sc == nullptr) {
				return invalidNameID;
			}
		}
	}
	
	TypeEx const& SymbolTable::findTypeByName(Token const& name) const {
		NameID const id = lookupName(name);
		if (id.category() != NameCategory::Type) {
			throw InvalidSymbolReference(name, id.category());
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
