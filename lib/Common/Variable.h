#ifndef SCATHA_COMMON_VARIABLE_H_
#define SCATHA_COMMON_VARIABLE_H_

#include "Basic/Basic.h"
#include "Common/Type.h"
#include "Common/Name.h"

namespace scatha {
	
	struct Variable {
		static constexpr std::string_view elementName() { return "Variable"; }
		
		explicit Variable(NameID nameID, TypeID typeID): _nameID(nameID), _typeID(typeID) {}
		NameID nameID() const { return _nameID; }
		TypeID typeID() const { return _typeID; }
		
	private:
		NameID _nameID;
		TypeID _typeID;
	};
	
}

#endif // SCATHA_COMMON_VARIABLE_H_

