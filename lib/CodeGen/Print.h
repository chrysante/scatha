#ifndef SCATHA_CODEGEN_PRINT_H_
#define SCATHA_CODEGEN_PRINT_H_

#include <span>
#include <iosfwd>

#include "Basic/Basic.h"

namespace scatha::codegen {
	
	struct PrintDescription {
		bool codeHasMarkers = false;
	};
	
	void printInstructions(std::span<u8 const>, std::ostream&, PrintDescription);
	
}

#endif // SCATHA_CODEGEN_PRINT_H_

