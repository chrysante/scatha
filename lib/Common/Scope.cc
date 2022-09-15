#include "Scope.h"

#include <sstream>

namespace scatha {
	
	/// MARK: ScopeError
	ScopeError::ScopeError(Scope const* scope, std::string_view name, Issue issue):
		std::runtime_error(makeMessage(scope, issue, name, NameID{})),
		_issue(issue)
	{
		SC_ASSERT(issue == NameAlreadyExists || issue == NameNotFound, "incompatible otherwise");
	}
	
	ScopeError::ScopeError(Scope const* scope, NameID id, Issue issue):
		std::runtime_error(makeMessage(scope, issue, {}, id)),
		_issue(issue)
	{
		SC_ASSERT(issue == IDNotFound, "incompatible otherwise");
	}
	
	ScopeError::ScopeError(Scope const* scope, std::string_view name, NameCategory newCat, NameCategory oldCat, Issue issue):
		std::runtime_error(makeMessage(scope, issue, name, {}, newCat, oldCat)),
		_issue(issue)
	{
		SC_ASSERT(issue == NameCategoryConflict, "incompatible otherwise");
	}
	
	std::string ScopeError::makeMessage(Scope const* scope, Issue issue,
										std::string_view name,
										NameID id,
										NameCategory newCat, NameCategory oldCat)
	{
		std::stringstream sstr;
		
		auto printFullName = [&](Scope const* sc) {
			while (true) {
				sstr << sc->name();
				if (sc->parentScope() == nullptr) { break; }
				sstr << '.';
				sc = sc->parentScope();
			}
		};
		
		switch (issue) {
			case NameAlreadyExists:
				sstr << "Name \"" << name << "\" already exists in scope: ";
				printFullName(scope);
				break;
				
			case NameNotFound:
				sstr << "Name \"" << name << "\" not found in scope: ";
				printFullName(scope);
				break;
				
			case IDNotFound:
				sstr << "ID \"" << id.id() << "\" not found in scope: ";
				printFullName(scope);
				break;
				
			case NameCategoryConflict:
				sstr << "Name \"" << name << "\" of category " << toString(newCat) << " was already declared as category " << toString(oldCat) << " in scope: ";
				printFullName(scope);
				break;
				
			default:
				break;
		}
		
		return sstr.str();
	}
	
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
	
	std::pair<NameID, bool> Scope::addName(std::string const& name, NameCategory category) {
		if (auto itr = _nameToID.find(name); itr != _nameToID.end()) {
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
		if (itr == _idToName.end()) {
			throw ScopeError(this, id, ScopeError::IDNotFound);
		}
		return itr->second;
	}
	
	NameID Scope::findIDByName(std::string const& name) const {
		auto result = tryFindIDByName(name);
		if (result) {
			return *result;
		}
		throw ScopeError(this, name, ScopeError::NameNotFound);
	}
	
	std::optional<NameID> Scope::tryFindIDByName(std::string const& name) const {
		auto itr = _nameToID.find(name);
		if (itr == _nameToID.end()) {
			return std::nullopt;
		}
		return itr->second;
	}
	
	Scope const* Scope::childScope(NameID id) const {
		auto itr = _childScopes.find(id);
		if (itr == _childScopes.end()) {
			throw ScopeError(this, id, ScopeError::IDNotFound);
		}
		return itr->second.get();
	}
	
}
