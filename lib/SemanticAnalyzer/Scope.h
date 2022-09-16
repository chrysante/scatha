#ifndef SCATHA_COMMON_SCOPED_H_
#define SCATHA_COMMON_SCOPED_H_

#include <memory>
#include <optional>
#include <string>
#include <stdexcept>

#include <utl/hashmap.hpp>
#include <utl/common.hpp>

#include "SemanticAnalyzer/SemanticElements.h"

namespace scatha::sem {
	
	/**
	 * class \p Scope
	 * Represents a scope like classes, namespaces and functions in the symbol table.
	 * Maintains a table mapping all names in a scope to NameIDs.
	 * Scopes are arranged in a tree structure where the global scope is the root.
	 */
	class Scope {
	public:
		enum Kind {
			Global, Function, Struct, Namespace, _count
		};
		
	public:
		explicit Scope(std::string name, Kind kind, Scope* parent);
		explicit Scope(std::string_view name, Kind kind, Scope* parent): Scope(std::string(name), kind, parent) {}
		~Scope() = default;
		
		Kind kind() const { return _kind; }
		
		std::string_view name() const { return _name; }
		
		// returns NameID and boolean == true iff name was just added / == false iff name already existed.
		std::pair<NameID, bool> addName(std::string_view, NameCategory);
		
		std::optional<NameID> findIDByName(std::string_view) const;
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
		Kind _kind;
		
		u64 _nameIDCounter = 0;
		
		std::string _name;
		utl::hashmap<std::string, NameID> _nameToID;
		utl::hashmap<NameID, std::string> _idToName;
		
		utl::hashmap<NameID, std::unique_ptr<Scope>> _childScopes;
	};
	
	std::string_view toString(Scope::Kind);
	
}

#endif // SCATHA_COMMON_SCOPED_H_

