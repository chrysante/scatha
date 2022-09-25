#ifndef SCATHA_CODEGEN_REGISTERDESCRIPTOR_H_
#define SCATHA_CODEGEN_REGISTERDESCRIPTOR_H_

#include <utl/hashmap.hpp>

#include "Assembly/Assembly.h"
#include "Basic/Basic.h"
#include "IC/ThreeAddressCode.h"
#include "Sema/SemanticElements.h"

namespace scatha::codegen {
	
	class RegisterDescriptor {
	public:
		void declareParameters(ic::FunctionLabel const&);
		
		assembly::RegisterIndex resolve(ic::Variable const&);
		assembly::RegisterIndex resolve(ic::Temporary const&);
		
		void clear();
		
	private:
		size_t index = 0;
		utl::hashmap<sema::SymbolID, size_t> variables;
		utl::hashmap<size_t, size_t> temporaries;
	};
	
}

#endif // SCATHA_CODEGEN_REGISTERDESCRIPTOR_H_

