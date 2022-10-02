#ifndef SCATHA_SEMA_VARIABLE_H_
#define SCATHA_SEMA_VARIABLE_H_

#include "Sema/EntityBase.h"

namespace scatha::sema {
	
	class Variable: public EntityBase {
	public:
		explicit Variable(std::string name, SymbolID symbolID, Scope* parentScope, TypeID typeID, bool isConstant):
			EntityBase(std::move(name), symbolID, parentScope),
			_typeID(typeID),
			_isConstant(isConstant)
		{}
		
		void setTypeID(TypeID id) { _typeID = id; }
		
		TypeID typeID() const { return _typeID; }
		bool isConstant() const { return _isConstant; }
		
	private:
		TypeID _typeID;
		bool _isConstant;
	};
	
}

#endif // SCATHA_SEMA_VARIABLE_H_

