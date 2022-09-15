#include "Name.h"

#include <utl/utility.hpp>

namespace scatha {
	
	std::string_view toString(NameCategory c) {
		using enum NameCategory;
		return UTL_SERIALIZE_ENUM(c, {
			{ None,      "None" },
			{ Type,      "Type" },
			{ Value,     "Value" },
			{ Namespace, "Namespace" },
			{ Function,  "Function" },
		});
	}
	
}
