#include "Scope.h"

#include <sstream>

namespace scatha::sem {
	
	/// MARK: Scope
	Scope::Scope(std::string name, Scope* parent):
		_parent(parent),
		_name(std::move(name))
	{
		if (this->parentScope() == nullptr) { // meaning we are the root
			_root = this;
		}
		else {
			_root = this->parentScope()->_root;
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
			case NameCategory::Function: [[fallthrough]];
			case NameCategory::Type:     [[fallthrough]];
			case NameCategory::Namespace: {
				bool const success = _childScopes.insert({ id, std::make_unique<Scope>(name, this) }).second;
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
	
}
