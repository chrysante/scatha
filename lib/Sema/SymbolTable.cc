#include "Sema/SymbolTable.h"

#include <string>

#include "Sema/SemanticIssue.h"

namespace scatha::sema {
	
	SymbolTable::SymbolTable():
		_globalScope(std::make_unique<GlobalScope>()),
		_currentScope(_globalScope.get())
	{
		_void   = addObjectType("void",   0, 0, true)->symbolID();
		_bool   = addObjectType("bool",   1, 1, true)->symbolID();
		_int    = addObjectType("int",    8, 8, true)->symbolID();
		_float  = addObjectType("float",  8, 8, true)->symbolID();
		_string = addObjectType("string", sizeof(std::string), alignof(std::string), true)->symbolID();
	}
	
	Expected<Function const&, SemanticIssue> SymbolTable::addFunction(std::string name, FunctionSignature sig) {
		SymbolID const overloadSetID = [&]{
			auto const id = currentScope().findID(name);
			if (id != SymbolID::Invalid) {
				return id;
			}
			// Create a new overload set
			auto [itr, success] = _overloadSets.insert(OverloadSet{
				name, generateID(), &currentScope()
			});
			SC_ASSERT(success, "");
			currentScope().add(*itr);
			return itr->symbolID();
		}();
		// Const cast here because we don't have a mutable getter
		auto* const overloadSetPtr = const_cast<OverloadSet*>(tryGetOverloadSet(overloadSetID));
		if (!overloadSetPtr) {
			return SemanticIssue{
				InvalidDeclaration(nullptr, InvalidDeclaration::Reason::Redeclaration,
								   currentScope(), SymbolCategory::Function)
			};
		}
		auto& overloadSet = *overloadSetPtr;
		auto const [itr, success] = overloadSet.add(Function(name, std::move(sig), generateID(), &currentScope()));
		auto& function = *itr;
		if (!success) {
			// 'function' references the existing function
			InvalidDeclaration::Reason const reason = function.signature().returnTypeID() == sig.returnTypeID() ?
				InvalidDeclaration::Reason::Redeclaration :
				InvalidDeclaration::Reason::CantOverloadOnReturnType;
			return SemanticIssue{
				InvalidDeclaration(nullptr, reason,
								   currentScope(), SymbolCategory::Function)
			};
		}
		auto const [_, fnPtrInsertSuccess] = _functions.insert({ function.symbolID(), &function });
		SC_ASSERT(fnPtrInsertSuccess, "");
		currentScope().add(function);
		return function;
	}

	Expected<Variable&, SemanticIssue> SymbolTable::addVariable(std::string name, TypeID typeID, bool isConstant) {
		SymbolID const symbolID = currentScope().findID(name);
		if (symbolID != SymbolID::Invalid) {
			return SemanticIssue{
				InvalidDeclaration(nullptr, InvalidDeclaration::Reason::Redeclaration,
								   currentScope(), SymbolCategory::Variable)
			};
		}
		auto [itr, success] = _variables.insert(Variable(name, generateID(), &currentScope(), typeID, isConstant));
		SC_ASSERT(success, "");
		currentScope().add(*itr);
		return *itr;
	}
	
	Expected<ObjectType&, SemanticIssue> SymbolTable::addObjectType(std::string name, size_t size, size_t align, bool isBuiltin) {
		SymbolID const symbolID = currentScope().findID(name);
		if (symbolID != SymbolID::Invalid) {
			return SemanticIssue{
				InvalidDeclaration(nullptr, InvalidDeclaration::Reason::Redeclaration,
								   currentScope(), SymbolCategory::ObjectType)
			};
		}
		auto [itr, success] = _objectTypes.insert(ObjectType(name, generateID(), &currentScope(), size, align, isBuiltin));
		SC_ASSERT(success, "");
		currentScope().add(*itr);
		return *itr;
	}
	
	Scope const& SymbolTable::addAnonymousScope() {
		auto [itr, success] = _anonymousScopes.insert(Scope(ScopeKind::Function, generateID(), &currentScope()));
		SC_ASSERT(success, "");
		currentScope().add(*itr);
		return *itr;
	}
	
	void SymbolTable::pushScope(SymbolID id) {
		auto const itr = currentScope()._children.find(id);
		SC_ASSERT(itr != currentScope()._children.end(), "not found");
		_currentScope = itr->second;
	}
	
	void SymbolTable::popScope() {
		_currentScope = currentScope().parent();
	}
	
	void SymbolTable::makeScopeCurrent(Scope* scope) {
		_currentScope = scope ? scope : &globalScope();
	}
	
	OverloadSet const& SymbolTable::getOverloadSet(SymbolID id) const {
		auto const* result = tryGetOverloadSet(id);
		SC_ASSERT(result, "ID must be valid and reference an overload set");
		return *result;
	}
	
	OverloadSet const* SymbolTable::tryGetOverloadSet(SymbolID id) const {
		auto const itr = _overloadSets.find(id);
		if (itr == _overloadSets.end()) { return nullptr; }
		return &*itr;
	}
	
	Function const& SymbolTable::getFunction(SymbolID id) const {
		auto const* result = tryGetFunction(id);
		SC_ASSERT(result, "ID must be valid and reference a function");
		return *result;
	}
	
	Function const* SymbolTable::tryGetFunction(SymbolID id) const {
		auto const itr = _functions.find(id);
		if (itr == _functions.end()) { return nullptr; }
		return itr->second;
	}
	
	Variable const& SymbolTable::getVariable(SymbolID id) const {
		auto const* result = tryGetVariable(id);
		SC_ASSERT(result, "ID must be valid and reference a variable");
		return *result;
	}
	
	Variable const* SymbolTable::tryGetVariable(SymbolID id) const {
		auto const itr = _variables.find(id);
		if (itr == _variables.end()) { return nullptr; }
		return &*itr;
	}
	
	ObjectType const& SymbolTable::getObjectType(SymbolID id) const {
		auto const* result = tryGetObjectType(id);
		SC_ASSERT(result, "ID must be valid and reference an object type");
		return *result;
	}
	
	ObjectType const* SymbolTable::tryGetObjectType(SymbolID id) const {
		auto const itr = _objectTypes.find(id);
		if (itr == _objectTypes.end()) { return nullptr; }
		return &*itr;
	}
	
	SymbolID SymbolTable::lookup(std::string_view name) const {
		Scope const* scope = &currentScope();
		while (scope != nullptr) {
			auto const id = scope->findID(name);
			if (id != SymbolID::Invalid) {
				return id;
			}
			scope = scope->parent();
		}
		return SymbolID::Invalid;
	}
	
	OverloadSet const* SymbolTable::lookupOverloadSet(std::string_view name) const {
		SymbolID const id = lookup(name);
		if (id == SymbolID::Invalid) { return nullptr; }
		return &getOverloadSet(id);
	}
	
	Variable const* SymbolTable::lookupVariable(std::string_view name) const {
		SymbolID const id = lookup(name);
		if (id == SymbolID::Invalid) { return nullptr; }
		return &getVariable(id);
	}
	
	ObjectType const* SymbolTable::lookupObjectType(std::string_view name) const {
		SymbolID const id = lookup(name);
		if (id == SymbolID::Invalid) { return nullptr; }
		return &getObjectType(id);
	}
	
	bool SymbolTable::is(SymbolID id, SymbolCategory cat) const {
		if (!std::has_single_bit(static_cast<unsigned>(cat))) {
			int const count = utl::log2(utl::to_underlying(SymbolCategory::_count) - 1);
			for (int i = 0; i < count; ++i) {
				if (test(SymbolCategory(1 << i) & cat) && is(id, SymbolCategory(1 << i))) {
					return true;
				}
			}
			return false;
		}
		
		using enum SymbolCategory;
		switch (cat) {
			case Variable:    return _variables.contains(id);
			case Namespace:   return false;
			case OverloadSet: return _overloadSets.contains(id);
			case Function:    return _functions.contains(id);
			case ObjectType:  return _objectTypes.contains(id);
			case _count: SC_DEBUGFAIL();
		}
	}
	
	SymbolCategory SymbolTable::categorize(SymbolID id) const {
		if (_variables.contains(id)) {
			return SymbolCategory::Variable;
		}
		if (_objectTypes.contains(id)) {
			return SymbolCategory::ObjectType;
		}
		if (_overloadSets.contains(id)) {
			return SymbolCategory::OverloadSet;
		}
		if (_functions.contains(id)) {
			return SymbolCategory::Function;
		}
		SC_DEBUGFAIL(); // What is this ID?
	}
	
	SymbolID SymbolTable::generateID() {
		return SymbolID(_idCounter++);
	}
	
}
