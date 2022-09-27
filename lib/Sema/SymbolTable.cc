#include "Sema/SymbolTable.h"

#include "Sema/SemanticIssue.h"

namespace scatha::sema {
	
	SymbolTable::SymbolTable() {
		_globalScope = std::make_unique<Scope>(std::string{}, Scope::Global, nullptr);
		_currentScope = _globalScope.get();
		
		// TODO: Find a better solution for this
		_void   = defineBuiltinType(Token("void"),    0, 0).id();
		_bool   = defineBuiltinType(Token("bool"),    1, 1).id();
		_int    = defineBuiltinType(Token("int"),     8, 8).id();
		_float  = defineBuiltinType(Token("float"),   8, 8).id();
		_string = defineBuiltinType(Token("string"), 24, 8).id();
	}
	
	std::pair<SymbolID, bool> SymbolTable::addSymbol(Token const& name, SymbolCategory cat) {
		auto const [symbolID, newlyAdded] = currentScope()->addSymbol(name.id, cat);
		if (!newlyAdded && symbolID.category() != cat) {
			throw InvalidRedeclaration(name, currentScope(), symbolID.category());
		}
		return { symbolID, newlyAdded };
	}
	
	SymbolID SymbolTable::addAnonymousSymbol(SymbolCategory category) {
		return currentScope()->addAnonymousSymbol(category);
	}
	
	void SymbolTable::pushScope(std::string_view name) {
		auto const id = currentScope()->findIDByName(name);
		SC_ASSERT(id, "Probably use of undeclared identifier, should have been handled before");
		pushScope(*id);
	}
	
	void SymbolTable::pushScope(SymbolID id) {
		_currentScope = currentScope()->childScope(id);
	}
	
	void SymbolTable::popScope() {
		_currentScope = currentScope()->parentScope();
		SC_ASSERT(currentScope() != nullptr, "Can't pop anymore as we are already in global scope");
	}
	
	SymbolID SymbolTable::declareType(Token const& name) {
		auto const [id, _] = addSymbol(name, SymbolCategory::Type);
		return id;
	}

	TypeEx& SymbolTable::defineType(Token const& name, size_t size, size_t align) {
		auto const [id, newlyAdded] = addSymbol(name, SymbolCategory::Type);
		
		auto [type, success] = types.emplace(id.id(), name.id, TypeID(id.id()), size, align);
		if (!success) {
/// MARK: Can't test this yet
			throw InvalidRedeclaration(name, currentScope());
		}
		SC_ASSERT_AUDIT(type != nullptr, "If the type has been added before it'd better not be null");
		return *type;
	}
	
	TypeEx& SymbolTable::defineBuiltinType(Token const& name, size_t size, size_t align) {
		auto& type = defineType(name, size, align);
		type._isBuiltin = true;
		return type;
	}
	
	std::pair<Function*, bool> SymbolTable::declareFunction(Token const& name, TypeID returnType, std::span<TypeID const> argumentTypes) {
		auto const [symbolID, newlyAdded] = addSymbol(name, SymbolCategory::Function);
		
		auto const computedFunctionTypeID = computeFunctionTypeID(returnType, argumentTypes);
		
		if (!newlyAdded) {
			throw InvalidFunctionDeclaration(name, currentScope());
		}
			
		// Since function types are not named, we use the TypeID as the key here. This should be fine, since function types cannot be found by name anyhow. So wen can just find them by their hashed signature aka their TypeID.
		[[maybe_unused]] auto [functionType, _] = types.emplace((u64)computedFunctionTypeID, returnType, argumentTypes, computedFunctionTypeID);
		auto const result = funcs.emplace(symbolID.id(), name.id, symbolID, computedFunctionTypeID);
		SC_ASSERT(result.second, "Function already exists");
		return result;
	}
	
	
	std::pair<Variable*, bool> SymbolTable::declareVariable(Token const& name, TypeID typeID, bool isConstant) {
		auto const [symbolID, newlyAdded] = addSymbol(name, SymbolCategory::Variable);
		return vars.emplace(symbolID.id(), name.id, symbolID, typeID, isConstant);
	}

	SymbolID SymbolTable::lookupName(Token const& name) const {
		std::optional<SymbolID> result;
		Scope const* sc = currentScope();
		while (true) {
			result = sc->findIDByName(name.id);
			if (result) {
				return *result;
			}
			sc = sc->parentScope();
			if (sc == nullptr) {
				return invalidSymbolID;
			}
		}
	}
	
	TypeEx const& SymbolTable::findTypeByName(Token const& name) const {
		SymbolID const id = lookupName(name);
		if (id.category() != SymbolCategory::Type) {
			throw InvalidSymbolReference(name, id.category());
		}
		return getType(id);
	}
	
	TypeEx const& SymbolTable::getType(TypeID id) const {
		return types.get((u64)id);
	}
	
	Function const& SymbolTable::getFunction(SymbolID id) const {
		return funcs.get(id.id());
	}
	
	Variable const& SymbolTable::getVariable(SymbolID id) const {
		return vars.get(id.id());
	}
	
}
