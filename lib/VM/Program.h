#ifndef SCATHA_VM_PROGRAM_H_
#define SCATHA_VM_PROGRAM_H_

#include <iosfwd>

#include <utl/vector.hpp>

#include "Basic/Basic.h"

namespace scatha::vm {
	
	class Program {
	public:
		friend void print(Program const&);
		friend void print(Program const&, std::ostream&);
		
		utl::vector<u8> instructions;
		utl::vector<u8> data;
	};
	
}

#endif // SCATHA_VM_PROGRAM_H_

