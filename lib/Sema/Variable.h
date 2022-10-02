#ifndef SCATHA_SEMA_VARIABLE_H_
#define SCATHA_SEMA_VARIABLE_H_

#include "Sema/EntityBase.h"

namespace scatha::sema {
	
	class Variable: public EntityBase {
	public:
		explicit Variable(std::string name, SymbolID symbolID, TypeID typeID, bool isConstant):
			EntityBase(std::move(name), symbolID),
			_typeID(typeID),
			_isConstant(isConstant)
		{}
		
		TypeID typeID() const { return _typeID; }
		
		bool isConstant() const { return _isConstant; }
		
	private:
		TypeID _typeID;
		bool _isConstant;
	};
	
}

#endif // SCATHA_SEMA_VARIABLE_H_

