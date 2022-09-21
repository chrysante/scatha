#include "Sema/Scope.h"

#include <sstream>
#include <ostream>

#include <utl/utility.hpp>

namespace scatha::sema {
	
	/// MARK: Scope
	Scope::Scope(std::string name, Kind kind, Scope* parent):
		_parent(parent),
		_kind(kind),
		_name(std::move(name))
	{
		if (this->parentScope() == nullptr) { // meaning we are the root / global scope
			SC_ASSERT(this->kind() == Global, "We must be root");
			_root = this;
		}
		else {
			SC_ASSERT(this->kind() != Global, "We must not be root");
			_root = this->parentScope()->_root;
		}
	}
	
	static Scope::Kind catToKind(SymbolCategory cat) {
		using enum SymbolCategory;
		SC_ASSERT(cat == Function || cat == Type || cat == Namespace, "Only these declare scopes");
		switch (cat) {
			case Function:
				return Scope::Function;
			case Type:
				return Scope::Struct;
			case Namespace:
				return Scope::Namespace;
				
			default:
				SC_UNREACHABLE();
		}
	}
	
	std::pair<SymbolID, bool> Scope::addSymbol(std::string_view name, SymbolCategory category) {
		return addSymbolImpl(name, category);
	}
	
	SymbolID Scope::addAnonymousSymbol(SymbolCategory category) {
		auto [result, success] = addSymbolImpl(std::nullopt, category);
		SC_ASSERT(success, "Anonymous symbols should always succeed");
		return result;
	}
	
	std::string Scope::findNameByID(SymbolID id) const {
		auto result = _nameIDMap.lookup(id);
		// Assert because this would be a bug and not caused by bad user code
		SC_ASSERT(result, "ID not found");
		return result.from();
	}
	
	std::optional<SymbolID> Scope::findIDByName(std::string_view name) const {
		auto result = _nameIDMap.lookup(std::string(name));
		if (!result) {
			return std::nullopt;
		}
		return result.to();
	}
	
	Scope const* Scope::childScope(SymbolID id) const {
		auto itr = _childScopes.find(id);
		SC_ASSERT(itr != _childScopes.end(), "ID not found");
		return itr->second.get();
	}
	
	std::pair<SymbolID, bool> Scope::addSymbolImpl(std::optional<std::string_view> name, SymbolCategory category) {
		if (name) {
			if (auto const result = _nameIDMap.lookup(std::string(*name))) {
				return { result.to(), false };
			}
		}
		
		auto const id = generateID(category);
		
		if (name) {
			auto insertResult = _nameIDMap.insert(std::string(*name), id);
			SC_ASSERT(insertResult, "Name should be available");
		}
		
		switch (category) {
				using enum SymbolCategory;
			case SymbolCategory::Function:  [[fallthrough]];
			case SymbolCategory::Type:      [[fallthrough]];
			case SymbolCategory::Namespace: {
				bool const success = _childScopes.insert({
					id,
					std::make_unique<Scope>(name ? *name : "__anonymous__", catToKind(category), this)
				}).second;
				SC_ASSERT(success, "ID should be available");
				break;
			}
			default:
				break;
		}
		
		return { id, true };
	}

	std::string_view toString(Scope::Kind kind) {
		return UTL_SERIALIZE_ENUM(kind, {
			{ Scope::Global,    "Global" },
			{ Scope::Function,  "Function" },
			{ Scope::Struct,    "Struct" },
			{ Scope::Namespace, "Namespace" },
			{ Scope::Anonymous, "Anonymous" },
		});
	}
	
}
