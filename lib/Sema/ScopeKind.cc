#include "ScopeKind.h"

#include <ostream>

#include <utl/utility.hpp>

namespace scatha::sema {
	
	std::string_view toString(ScopeKind k) {
		return UTL_SERIALIZE_ENUM(k, {
			{ ScopeKind::Global,    "Global" },
			{ ScopeKind::Namespace, "Namespace" },
			{ ScopeKind::Variable,  "Variable" },
			{ ScopeKind::Function,  "Function" },
			{ ScopeKind::Object,    "Object" },
			{ ScopeKind::Anonymous, "Anonymous" },
		});
	}
	
	std::ostream& operator<<(std::ostream& str, ScopeKind k) {
		return str << toString(k);
	}
	
}
