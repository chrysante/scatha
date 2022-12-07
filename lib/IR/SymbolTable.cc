#include "SymbolTable.h"

using namespace scatha;
using namespace ir;

SymbolTable::SymbolTable() {
    
}

SymbolID SymbolTable::add(Entity* entity, SymbolKind kind) {
    SymbolID const symbolID(_idCounter++, kind);
    _entities.insert({ symbolID, entity });
    return symbolID;
}

Entity const* SymbolTable::get(SymbolID id) const {
    auto const itr = _entities.find(id);
    SC_ASSERT(itr != _entities.end(), "ID not found.");
    return itr->second;
}
