#ifndef SCATHA_SEMA_SCOPE_H_
#define SCATHA_SEMA_SCOPE_H_

#include <memory>

#include <utl/hashmap.hpp>

#include "Sema/EntityBase.h"
#include "Sema/ScopeKind.h"

namespace scatha::sema {
	
	namespace internal { class ScopePrinter; }
	
	class SCATHA(API) Scope: public EntityBase {
	public:
		ScopeKind kind() const { return _kind; }
		explicit Scope(ScopeKind, SymbolID symbolID, Scope* parent);
		
	protected:
	public:
		explicit Scope(ScopeKind, std::string name, SymbolID symbolID, Scope* parent);
		
		// Until we have heterogenous lookup
		SymbolID findID(std::string_view name) const;
		
	private:
		friend class internal::ScopePrinter;
		friend class SymbolTable;
		void add(EntityBase const& entity);
		void add(Scope& scopingEntity);
		
	private:
		// Scopes don't own their childscopes. These objects are owned by the symbol table.
		utl::hashmap<SymbolID, Scope*> _children;
		utl::hashmap<std::string, SymbolID> _symbols;
		ScopeKind _kind;
	};
	
	class GlobalScope: public Scope {
	public:
		GlobalScope();
	};
	
}

#endif // SCATHA_SEMA_SCOPE_H_

