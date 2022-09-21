#ifndef SCATHA_VM_PROGRAM_H_
#define SCATHA_VM_PROGRAM_H_

#include <utl/vector.hpp>

#include "Basic/Basic.h"

namespace scatha::vm {
	
	class Program {
	public:
		utl::vector<u8> instructions;
		utl::vector<u8> data;
	};
	
}

#endif // SCATHA_VM_PROGRAM_H_

