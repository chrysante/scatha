#include "Common/SourceLocation.h"

#include <ostream>
#include <iomanip>

namespace scatha {
	
	std::ostream& operator<<(std::ostream& str, SourceLocation const& sc) {
		return str << "(" << std::setw(3) << sc.line << ", " << std::setw(3) << sc.column << ")";
	}
	
}
