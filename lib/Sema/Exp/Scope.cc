#include "Sema/Exp/Scope.h"

namespace scatha::sema::exp {
	
	Scope::Scope(ScopeKind kind, std::string name, SymbolID symbolID, Scope* parent):
		EntityBase(std::move(name), symbolID),
		_kind(kind),
		_parent(parent)
	{
		
	}
	
	SymbolID Scope::findID(std::string const& name) const {
		auto const itr = _symbols.find(name);
		return itr == _symbols.end() ? SymbolID::Invalid : itr->second;
	}
	
	SymbolID Scope::findID(std::string_view name) const { return findID(std::string(name)); }

	void Scope::add(EntityBase const& entity) {
		auto const [itr, success] = _symbols.insert({ std::string(entity.name()), entity.symbolID() });
		SC_ASSERT(success, "");
	}
	
	void Scope::add(Scope& scopingEntity) {
		add(static_cast<EntityBase const&>(scopingEntity));
		auto const [itr, success] = _children.insert({
			scopingEntity.symbolID(),
			&scopingEntity
		});
		SC_ASSERT(success, "");
	}
	
	GlobalScope::GlobalScope():
		Scope(ScopeKind::Global, "__GLOBAL__", SymbolID::Invalid, nullptr)
	{
		
	}
	
}
