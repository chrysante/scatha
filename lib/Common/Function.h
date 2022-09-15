#ifndef SCATHA_COMMON_FUNCTION_H_
#define SCATHA_COMMON_FUNCTION_H_

#include "Common/Type.h"
#include "Common/Name.h"

namespace scatha {
	
	struct Function {
		static constexpr std::string_view elementName() { return "Function"; }
		
		explicit Function(NameID name): _nameID(name) {}
		
		void setTypeID(TypeID id) { _typeID = id; }
		
		NameID nameID() const { return _nameID; }
		TypeID typeID() const { return _typeID; }
		
	private:
		NameID _nameID;
		TypeID _typeID = TypeID::Invalid;
	};
	
}

#endif // SCATHA_COMMON_FUNCTION_H_

