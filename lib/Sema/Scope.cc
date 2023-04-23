#include "Sema/Scope.h"

namespace scatha::sema {

Scope::Scope(EntityType entityType,
             ScopeKind kind,
             std::string name,
             SymbolID symbolID,
             Scope* parent):
    Entity(entityType, std::move(name), symbolID, parent), _kind(kind) {}

SymbolID Scope::findID(std::string_view name) const {
    auto const itr = _symbols.find(std::string(name));
    return itr == _symbols.end() ? SymbolID::Invalid : itr->second;
}

void Scope::add(Entity* entity) {
    auto impl = [this](Entity& entity) {
        auto const [itr, success] =
            _symbols.insert({ std::string(entity.name()), entity.symbolID() });
        SC_ASSERT(success, "");
    };
    if (auto* scope = dyncast<Scope*>(entity)) {
        if (!scope->isAnonymous() &&
            /// Can't add functions here because of name collisions due to
            /// overloading.
            scope->kind() != ScopeKind::Function)
        {
            impl(*scope);
        }
        auto const [itr, success] =
            _children.insert({ scope->symbolID(), scope });
        SC_ASSERT(success, "");
    }
    else {
        impl(*entity);
    }
}

AnonymousScope::AnonymousScope(SymbolID id, ScopeKind scopeKind, Scope* parent):
    Scope(EntityType::AnonymousScope, scopeKind, std::string{}, id, parent) {}

GlobalScope::GlobalScope(SymbolID id):
    Scope(EntityType::GlobalScope,
          ScopeKind::Global,
          "__GLOBAL__",
          id,
          nullptr) {}

} // namespace scatha::sema
