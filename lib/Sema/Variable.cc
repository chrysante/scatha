#include "Sema/Variable.h"

#include "Sema/Scope.h"

using namespace scatha;

sema::Variable::Variable(std::string name, SymbolID symbolID, Scope* parentScope, TypeID typeID, size_t offset):
    EntityBase(std::move(name), symbolID, parentScope), _typeID(typeID), _offset(offset) {}

bool sema::Variable::isLocal() const {
    return parent()->kind() == ScopeKind::Function || parent()->kind() == ScopeKind::Anonymous;
}
