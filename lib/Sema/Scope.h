#ifndef SCATHA_COMMON_SCOPED_H_
#define SCATHA_COMMON_SCOPED_H_

#include <memory>
#include <optional>
#include <string>
#include <stdexcept>
#include <iosfwd>

#include <utl/hashmap.hpp>
#include <utl/common.hpp>

#include "Basic/Bimap.h"
#include "Sema/SemanticElements.h"

namespace scatha::sema {
	
	namespace internal { class ScopePrinter; }
	
	/**
	 * class \p Scope
	 * Represents a scope like classes, namespaces and functions in the symbol table.
	 * Maintains a table mapping all names in a scope to SymbolIDs.
	 * Scopes are arranged in a tree structure where the global scope is the root.
	 */
	class Scope {
	public:
		enum Kind {
			Global, Function, Struct, Namespace, Anonymous, _count
		};
		
	public:
		explicit Scope(std::string name, Kind kind, Scope* parent);
		explicit Scope(std::string_view name, Kind kind, Scope* parent): Scope(std::string(name), kind, parent) {}
		~Scope() = default;
		
		Kind kind() const { return _kind; }
		
		std::string_view name() const { return _name; }
		
		// returns SymbolID and boolean == true iff name was just added / == false iff name already existed.
		std::pair<SymbolID, bool> addSymbol(std::string_view, SymbolCategory);
		SymbolID addAnonymousSymbol(SymbolCategory);
		
		std::optional<SymbolID> findIDByName(std::string_view) const;
		std::string findNameByID(SymbolID) const;
		
		Scope* parentScope() { return _parent; }
		Scope const* parentScope() const { return _parent; }
		Scope* childScope(SymbolID id) { return &utl::as_mutable(*utl::as_const(*this).childScope(id)); }
		Scope const* childScope(SymbolID id) const;
		
	private:
		friend class internal::ScopePrinter;
		
		auto generateID(SymbolCategory cat) { return SymbolID(++(_root->_symbolIDCounter), cat); }
		std::pair<SymbolID, bool> addSymbolImpl(std::optional<std::string_view>, SymbolCategory);
		
	private:
		Scope* _parent = nullptr;
		Scope* _root = nullptr;
		Kind _kind;
		
		u64 _symbolIDCounter = 0; // must only be accessed via the _root pointer so we don't generate the same ID twice
		
		std::string _name;
		Bimap<std::string, SymbolID> _nameIDMap;
		
		utl::hashmap<SymbolID, std::unique_ptr<Scope>> _childScopes;
	};
	
	std::string_view toString(Scope::Kind);
	
}

#endif // SCATHA_COMMON_SCOPED_H_

