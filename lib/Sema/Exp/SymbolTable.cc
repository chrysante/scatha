#include "Sema/Exp/SymbolTable.h"

#include "Sema/SemanticIssue.h"

namespace scatha::sema::exp {
	
	SymbolTable::SymbolTable():
		_globalScope(std::make_unique<GlobalScope>()),
		_currentScope(_globalScope.get())
	{
		_void  = addObjectType("void",  0, 0)->symbolID();
		_bool  = addObjectType("bool",  1, 1)->symbolID();
		_int   = addObjectType("int",   8, 8)->symbolID();
		_float = addObjectType("float", 8, 8)->symbolID();
	}
	
	Expected<Function const&, OverloadIssue> SymbolTable::addFunction(std::string name, FunctionSignature sig) {
		SymbolID const overloadSetID = [&]{
			auto const id = currentScope().findID(name);
			if (id != SymbolID::Invalid) {
				return id;
			}
			// Create a new overload set
			auto [itr, success] = _overloadSets.insert(OverloadSet{
				name, generateID()
			});
			SC_ASSERT(success, "");
			currentScope().add(*itr);
			return itr->symbolID();
		}();
		
		auto& overloadSet = const_cast<OverloadSet&>(getOverloadSet(overloadSetID));
		auto const [itr, success] = overloadSet.add(Function(name, std::move(sig), generateID(), &currentScope()));
		auto& function = *itr;
		if (!success) {
			// 'function' references the existing function
			OverloadIssue::Reason const reason = function.signature().returnTypeID() == sig.returnTypeID() ?
				OverloadIssue::Redefinition :
				OverloadIssue::CantOverloadOnReturnType;
			return OverloadIssue(name, function.symbolID(), reason);
		}
		return function;
	}

	Expected<Variable const&, SymbolCollisionIssue> SymbolTable::addVariable(std::string name, TypeID typeID) {
		SymbolID const symbolID = currentScope().findID(name);
		if (symbolID != SymbolID::Invalid) {
			return SymbolCollisionIssue(name, symbolID);
		}
		auto [itr, success] = _variables.insert(Variable(name, generateID(), typeID));
		SC_ASSERT(success, "");
		currentScope().add(*itr);
		return *itr;
	}
	
	Expected<ObjectType&, SymbolCollisionIssue> SymbolTable::addObjectType(std::string name, size_t size, size_t align) {
		SymbolID const symbolID = currentScope().findID(name);
		if (symbolID != SymbolID::Invalid) {
			return SymbolCollisionIssue(name, symbolID);
		}
		auto [itr, success] = _objectTypes.insert(ObjectType(name, generateID(), &currentScope(), size, align));
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
		_currentScope = currentScope()._parent;
	}
	
	OverloadSet const& SymbolTable::getOverloadSet(SymbolID id) const {
		auto const itr = _overloadSets.find(id);
		SC_ASSERT(itr != _overloadSets.end(), "ID must be valid and reference an overload set");
		return *itr;
	}
	
	Variable const& SymbolTable::getVariable(SymbolID id) const {
		auto const itr = _variables.find(id);
		SC_ASSERT(itr != _variables.end(), "ID must be valid and reference a variable");
		return *itr;
	}
	
	ObjectType const& SymbolTable::getObjectType(SymbolID id) const {
		auto const itr = _objectTypes.find(id);
		SC_ASSERT(itr != _objectTypes.end(), "ID must be valid and reference an object type");
		return *itr;
	}
	
	SymbolID SymbolTable::lookup(std::string_view name) const {
		Scope const* scope = &currentScope();
		while (scope != nullptr) {
			auto const id = scope->findID(name);
			if (id != SymbolID::Invalid) {
				return id;
			}
			scope = scope->_parent;
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
	
	SymbolID SymbolTable::generateID() {
		return SymbolID(_idCounter++);
	}
	
}
