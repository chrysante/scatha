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
		void generateBinaryExpression(assembly::AssemblyStream&, ic::ThreeAddressStatement const&);
		
		struct ResolvedArg {
			CodeGenerator& self;
			ic::TasArgument const& arg;
			void streamInsert(assembly::AssemblyStream&) const;
		};
		friend struct ResolvedArg;
		friend assembly::AssemblyStream& operator<<(assembly::AssemblyStream&, ResolvedArg);
		
		ResolvedArg resolve(ic::TasArgument const&);
		size_t countRegisters(size_t index) const;
		
	private:
		size_t paramIndex = 0;
		ic::ThreeAddressCode const& tac;
		RegisterDescriptor rd;
	};
	
}

#endif // SCATHA_CODEGEN_CODEGENERATOR_H_

