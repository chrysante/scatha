#ifndef SCATHA_CODEGEN_REGISTERDESCRIPTOR_H_
#define SCATHA_CODEGEN_REGISTERDESCRIPTOR_H_

#include <optional>

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
		
		std::optional<assembly::RegisterIndex> resolve(ic::TasArgument const&);
		
		void clear();
		
		size_t currentIndex() const { return index; }
		
	private:
		size_t index = 0;
		utl::hashmap<sema::SymbolID, size_t> variables;
		utl::hashmap<size_t, size_t> temporaries;
	};
	
}

#endif // SCATHA_CODEGEN_REGISTERDESCRIPTOR_H_

