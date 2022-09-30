#ifndef SCATHA_SEMA_VARIABLE_H_
#define SCATHA_SEMA_VARIABLE_H_

#include "Sema/Exp/EntityBase.h"

namespace scatha::sema::exp {
	
	class Variable: public EntityBase {
	public:
		explicit Variable(std::string name, SymbolID symbolID, TypeID typeID):
			EntityBase(std::move(name), symbolID),
			_typeID(typeID)
		{}
		
		TypeID typeID() const { return _typeID; }
		
	private:
		TypeID _typeID;
	};
	
}

#endif // SCATHA_SEMA_VARIABLE_H_

