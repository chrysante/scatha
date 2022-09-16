#include "Scope.h"

#include <sstream>

#include <utl/utility.hpp>

namespace scatha::sem {
	
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
	
	static Scope::Kind catToKind(NameCategory cat) {
		using enum NameCategory;
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
	
	std::pair<NameID, bool> Scope::addName(std::string_view name, NameCategory category) {
		if (auto itr = _nameToID.find(std::string(name)); itr != _nameToID.end()) {
			auto const id = itr->second;
			SC_ASSERT(_idToName.contains(id),
					  "if it exists in the other map it also must exists in this map.\n"
					  "maybe create a bimap type to abstract this behaviour");
			return { id, false };
		}
		auto const id = generateID(category);
		bool const ntiInsert = _nameToID.insert({ name, id }).second;
		SC_ASSERT(ntiInsert, "Name should be available");
		bool const itnInsert = _idToName.insert({ id, name }).second;
		SC_ASSERT(itnInsert, "ID should be available");
		
		switch (category) {
				using enum NameCategory;
			case NameCategory::Function:  [[fallthrough]];
			case NameCategory::Type:      [[fallthrough]];
			case NameCategory::Namespace: {
				bool const success = _childScopes.insert({ id, std::make_unique<Scope>(name, catToKind(category), this) }).second;
				SC_ASSERT(success, "Name should be available");
				break;
			}
			default:
				break;
		}
		
		return { id, true };
	}
	
	std::string Scope::findNameByID(NameID id) const {
		auto itr = _idToName.find(id);
		// Assert because this would be a bug and not caused by bad user code
		SC_ASSERT(itr != _idToName.end(), "ID not found");
		return itr->second;
	}
	
	std::optional<NameID> Scope::findIDByName(std::string_view name) const {
		auto itr = _nameToID.find(std::string(name));
		if (itr == _nameToID.end()) {
			return std::nullopt;
		}
		return itr->second;
	}
	
	Scope const* Scope::childScope(NameID id) const {
		auto itr = _childScopes.find(id);
		SC_ASSERT(itr != _childScopes.end(), "ID not found");
		return itr->second.get();
	}

	std::string_view toString(Scope::Kind kind) {
		return UTL_SERIALIZE_ENUM(kind, {
			{ Scope::Global,    "Global" },
			{ Scope::Function,  "Function" },
			{ Scope::Struct,    "Struct" },
			{ Scope::Namespace, "Namespace" },
		});
	}
	
}
