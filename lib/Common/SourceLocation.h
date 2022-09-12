#pragma once

#ifndef SCATHA_COMMON_SOURCELOCATION_H_
#define SCATHA_COMMON_SOURCELOCATION_H_

#include <iosfwd>

#include "Basic/Basic.h"

namespace scatha {
	
	struct SourceLocation {
		u64 index = 0;
		u32 line = 0, column = 0;
	};

	std::ostream& operator<<(std::ostream&, SourceLocation const&);
	
}

#endif // SCATHA_COMMON_SOURCELOCATION_H_
