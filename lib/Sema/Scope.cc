#include "Sema/Scope.h"

namespace scatha::sema {

Scope::Scope(ScopeKind kind, SymbolID symbolID, Scope* parent):
    Scope(kind, {}, symbolID, parent) {}

Scope::Scope(ScopeKind kind,
             std::string name,
             SymbolID symbolID,
             Scope* parent):
    EntityBase(std::move(name), symbolID, parent), _kind(kind) {}

SymbolID Scope::findID(std::string_view name) const {
    auto const itr = _symbols.find(std::string(name));
    return itr == _symbols.end() ? SymbolID::Invalid : itr->second;
}

void Scope::add(EntityBase& entity) {
    auto const [itr, success] =
        _symbols.insert({ std::string(entity.name()), entity.symbolID() });
    SC_ASSERT(success, "");
}

void Scope::add(Scope& scopingEntity) {
    if (!scopingEntity.isAnonymous() &&
        /// Can't add functions here because of name collisions due to
        /// overloading.
        scopingEntity.kind() != ScopeKind::Function)
    {
        add(static_cast<EntityBase&>(scopingEntity));
    }
    auto const [itr, success] =
        _children.insert({ scopingEntity.symbolID(), &scopingEntity });
    SC_ASSERT(success, "");
}

GlobalScope::GlobalScope():
    Scope(ScopeKind::Global, "__GLOBAL__", SymbolID::Invalid, nullptr) {}

} // namespace scatha::sema
