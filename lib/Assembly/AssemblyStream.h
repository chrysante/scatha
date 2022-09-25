#ifndef SCATHA_ASSEMBLY_ASSEMBLYSTREAM_H_
#define SCATHA_ASSEMBLY_ASSEMBLYSTREAM_H_

#include <array>

#include <utl/vector.hpp>

#include "Assembly/Assembly.h"
#include "Basic/Basic.h"

namespace scatha::assembly {
	
	class AssemblyStream {
		friend class Assembler;
		
	public:
		/** MARK: operator<<
		 * Family of functions for inserting data into the stream
		 */
		friend AssemblyStream& operator<<(AssemblyStream&, Instruction);
		friend AssemblyStream& operator<<(AssemblyStream&, Value8);
		friend AssemblyStream& operator<<(AssemblyStream&, Value16);
		friend AssemblyStream& operator<<(AssemblyStream&, Value32);
		friend AssemblyStream& operator<<(AssemblyStream&, Value64);
		friend AssemblyStream& operator<<(AssemblyStream&, RegisterIndex);
		friend AssemblyStream& operator<<(AssemblyStream&, MemoryAddress);
		friend AssemblyStream& operator<<(AssemblyStream&, Label);
		
	private:
		/// Data insertion helpers
		void insert(u8);
		template <size_t N>
		void insert(std::array<u8, N>);
		void insert(assembly::Marker);
		
	private:
		utl::vector<u8> data;
	};
	
}

#endif // SCATHA_ASSEMBLY_ASSEMBLYSTREAM_H_

