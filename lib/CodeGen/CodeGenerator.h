#ifndef SCATHA_CODEGEN_CODEGENERATOR_H_
#define SCATHA_CODEGEN_CODEGENERATOR_H_

#include "Assembly/AssemblyStream.h"
#include "Basic/Basic.h"
#include "IC/ThreeAddressCode.h"
#include "CodeGen/RegisterDescriptor.h"

namespace scatha::codegen {
	
	class CodeGenerator {
	public:
		explicit CodeGenerator(ic::ThreeAddressCode const&);
		
		assembly::AssemblyStream run();
		
	private:
		void submit(assembly::AssemblyStream&, ic::TasArgument const&);
		
	private:
		ic::ThreeAddressCode const& tac;
		RegisterDescriptor rd;
	};
	
}

#endif // SCATHA_CODEGEN_CODEGENERATOR_H_

