#ifndef SCATHA_SEMA_ENTITYBASE_H_
#define SCATHA_SEMA_ENTITYBASE_H_

#include <string>
#include <string_view>

#include "Basic/Basic.h"
#include "Sema/SymbolID.h"

namespace scatha::sema {
	
	class Scope;
	
	/**
	 * Base class for all entities in the language.
	 *
	 */
	class SCATHA(API) EntityBase {
	public:
		struct MapHash: std::hash<SymbolID> {
			struct is_transparent;
			using std::hash<SymbolID>::operator();
			size_t operator()(EntityBase const& e) const {
				return std::hash<SymbolID>{}(e.symbolID());
			}
		};
		
		struct MapEqual {
			struct is_transparent;
			bool operator()(EntityBase const& a, EntityBase const& b) const {
				return a.symbolID() == b.symbolID();
			}
			bool operator()(EntityBase const& a, SymbolID b) const {
				return a.symbolID() == b;
			}
			bool operator()(SymbolID a, EntityBase const& b) const {
				return a == b.symbolID();
			}
		};
		
	public:
		std::string_view name() const { return _name; }
		SymbolID symbolID() const { return _symbolID; }
		
		bool isAnonymous() const { return _name.empty(); }
		
		Scope* parent() { return _parent; }
		Scope const* parent() const { return _parent; }
		
	protected:
		explicit EntityBase(std::string name, SymbolID symbolID, Scope* parent):
			_name(name),
			_symbolID(symbolID),
			_parent(parent)
		{}
		~EntityBase() = default;
		
	private:
		std::string _name;
		SymbolID _symbolID;
		Scope* _parent = nullptr;
	};
	
}

#endif // SCATHA_SEMA_ENTITYBASE_H_

