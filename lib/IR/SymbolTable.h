#ifndef SCATHA_IR_SYMBOLTABLE_H_
#define SCATHA_IR_SYMBOLTABLE_H_

#include <utl/hashmap.hpp>

#include "Basic/Basic.h"
#include "IR/Fwd.h"
#include "IR/SymbolID.h"

namespace scatha::ir {

class SCATHA(API) SymbolTable {
public:
    SymbolTable();
    SymbolID add(Entity* entity, SymbolKind kind);
    Entity const* get(SymbolID symbolID) const;
    
    
private:
    utl::hashmap<SymbolID, Entity*> _entities;
    u64 _idCounter = 0;
};
	
} // namespace scatha::ir

#endif // SCATHA_IR_SYMBOLTABLE_H_

