#include "Sema/Variable.h"

#include "Sema/Scope.h"

using namespace scatha;

sema::Variable::Variable(std::string name,
                         SymbolID symbolID,
                         Scope* parentScope,
                         QualType const* type):
    Entity(EntityType::Variable, std::move(name), symbolID, parentScope),
    _type(type) {}

bool sema::Variable::isLocal() const {
    return parent()->kind() == ScopeKind::Function ||
           parent()->kind() == ScopeKind::Anonymous;
}
