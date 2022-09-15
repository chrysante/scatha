#ifndef SCATHA_COMMON_SCOPED_H_
#define SCATHA_COMMON_SCOPED_H_

#include <memory>
#include <optional>
#include <string>
#include <stdexcept>

#include <utl/hashmap.hpp>
#include <utl/common.hpp>

#include "Common/Name.h"

namespace scatha {
	
	/// MARK: ScopeError
	class ScopeError: std::runtime_error {
	public:
		enum Issue {
			NameAlreadyExists, NameNotFound, IDNotFound, NameCategoryConflict
		};
		
	public:
		ScopeError(class Scope const*, std::string_view name, Issue);
		ScopeError(class Scope const*, NameID nameID, Issue);
		ScopeError(class Scope const*, std::string_view name, NameCategory newCat, NameCategory oldCat, Issue);
		
		Issue issue() const { return _issue; }
		
	private:
		static std::string makeMessage(Scope const*, Issue,
									   std::string_view name,
									   NameID,
									   NameCategory newCat = {}, NameCategory oldCat = {});
		
	private:
		Issue _issue{};
	};
	
	/// MARK: Scope
	class Scope {
	public:
		explicit Scope(std::string name, Scope* parent);
		~Scope() = default;
		
		std::string_view name() const { return _name; }
		
		// returns NameID and boolean == true iff name was just added / == false iff name already existed.
		std::pair<NameID, bool> addName(std::string const&, NameCategory);
		
		NameID findIDByName(std::string const&) const;
		std::optional<NameID> tryFindIDByName(std::string const&) const;
		std::string findNameByID(NameID) const;
		
		Scope* parentScope() { return _parent; }
		Scope const* parentScope() const { return _parent; }
		Scope* childScope(NameID id) { return &utl::as_mutable(*utl::as_const(*this).childScope(id)); }
		Scope const* childScope(NameID id) const;
		
	private:
		auto generateID(NameCategory cat) { return NameID(++(_root->_nameIDCounter), cat); }
		
	private:
		Scope* _parent = nullptr;
		Scope* _root = nullptr;
		u64 _nameIDCounter = 0;
		
		std::string _name;
		utl::hashmap<std::string, NameID> _nameToID;
		utl::hashmap<NameID, std::string> _idToName;
		
		utl::hashmap<NameID, std::unique_ptr<Scope>> _childScopes;
	};
	
}

#endif // SCATHA_COMMON_SCOPED_H_

