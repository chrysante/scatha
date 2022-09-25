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
		void generateBinaryArithmetic(assembly::AssemblyStream&, ic::ThreeAddressStatement const&);
		void generateComparison(assembly::AssemblyStream&, ic::ThreeAddressStatement const&);
		void generateJump(assembly::AssemblyStream&, ic::ThreeAddressStatement const&);
		
		struct ResolvedArg {
			CodeGenerator& self;
			ic::TasArgument const& arg;
			void streamInsert(assembly::AssemblyStream&) const;
		};
		friend struct ResolvedArg;
		friend assembly::AssemblyStream& operator<<(assembly::AssemblyStream&, ResolvedArg);
		
		ResolvedArg resolve(ic::TasArgument const&);
		
		struct FunctionRegisterCount {
			size_t local = 0;
			size_t maxFcParams = 0;
			
			size_t total() const { return local + 2 + maxFcParams; }
		};
		
		FunctionRegisterCount countRequiredRegistersForFunction(size_t index) const;
		
	private:
		ic::ThreeAddressCode const& tac;
		RegisterDescriptor rd;
		// Index of the current function parameter.
		// Will be reset to 0 when a call instruction is encountered.
		size_t paramIndex = 0;
		
		// Number of required registers for the current function.
		// Will be set when a function label is encountered.
		FunctionRegisterCount requiredRegisters;
	};
	
}

#endif // SCATHA_CODEGEN_CODEGENERATOR_H_

