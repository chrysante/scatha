#ifndef SCATHA_SEMA_SCOPE_H_
#define SCATHA_SEMA_SCOPE_H_

#include <memory>

#include <utl/hashmap.hpp>

#include "Sema/Exp/EntityBase.h"
#include "Sema/Exp/ScopeKind.h"

namespace scatha::sema::exp {
	
	class Scope: public EntityBase {
	protected:
		explicit Scope(ScopeKind, std::string name, SymbolID symbolID, Scope* parent);
		
		~Scope() = default;
		
		// Until we have heterogenous lookup
		SymbolID findID(std::string const& name) const;
		SymbolID findID(std::string_view name) const;
		
		ScopeKind kind() const { return _kind; }
		
	private:
		friend class SymbolTable;
		void add(EntityBase const& entity);
		void add(Scope& scopingEntity);
		
	private:
		// Scopes don't own their childscopes. These objects are owned by the symbol table.
		utl::hashmap<SymbolID, Scope*> _children;
		utl::hashmap<std::string, SymbolID> _symbols;
		ScopeKind _kind;
		Scope* _parent = nullptr;
	};
	
	class GlobalScope: public Scope {
	public:
		GlobalScope();
	};
	
}

#endif // SCATHA_SEMA_SCOPE_H_

