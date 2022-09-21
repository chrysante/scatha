#include "VM/Program.h"

#include <iostream>

#include "CodeGen/Print.h"

namespace scatha::vm {
	
	void print(Program const& p) {
		print(p, std::cout);
	}
	
	void print(Program const& p, std::ostream& str) {
		codegen::printInstructions(p.instructions, str, { .codeHasMarkers = false });
	}
	
}
