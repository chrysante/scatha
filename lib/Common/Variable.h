#ifndef SCATHA_COMMON_VARIABLE_H_
#define SCATHA_COMMON_VARIABLE_H_

#include "Basic/Basic.h"
#include "Common/Type.h"

namespace scatha {
	
	struct Variable {
		static constexpr std::string_view elementName() { return "Variable"; }
		
		explicit Variable(TypeID type): _type(type) {}
		TypeID type() const { return _type; }
		
	private:
		TypeID _type;
	};
	
}

#endif // SCATHA_COMMON_VARIABLE_H_

