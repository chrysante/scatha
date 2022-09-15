#ifndef SCATHA_COMMON_FUNCTION_H_
#define SCATHA_COMMON_FUNCTION_H_

#include "Common/Type.h"

namespace scatha {
	
	struct Function {
		static constexpr std::string_view elementName() { return "Function"; }
		
		explicit Function(TypeID type): _type(type) {}
		TypeID type() const { return _type; }
		
	private:
		TypeID _type;
	};
	
}

#endif // SCATHA_COMMON_FUNCTION_H_

