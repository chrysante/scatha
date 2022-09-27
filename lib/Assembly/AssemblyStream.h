#ifndef SCATHA_ASSEMBLY_ASSEMBLYSTREAM_H_
#define SCATHA_ASSEMBLY_ASSEMBLYSTREAM_H_

#include <array>

#include <utl/vector.hpp>

#include "Assembly/Assembly.h"
#include "Basic/Basic.h"

namespace scatha::assembly {
		
	class SCATHA(API) AssemblyStream {
		friend class StreamIterator;
		
	public:
		/** MARK: operator<<
		 * Family of functions for inserting data into the stream
		 */
		SCATHA(API) friend AssemblyStream& operator<<(AssemblyStream&, Instruction);
		SCATHA(API) friend AssemblyStream& operator<<(AssemblyStream&, Value8);
		SCATHA(API) friend AssemblyStream& operator<<(AssemblyStream&, Value16);
		SCATHA(API) friend AssemblyStream& operator<<(AssemblyStream&, Value32);
		SCATHA(API) friend AssemblyStream& operator<<(AssemblyStream&, Value64);
		SCATHA(API) friend AssemblyStream& operator<<(AssemblyStream&, RegisterIndex);
		SCATHA(API) friend AssemblyStream& operator<<(AssemblyStream&, MemoryAddress);
		SCATHA(API) friend AssemblyStream& operator<<(AssemblyStream&, Label);
		
		u8& operator[](size_t index) { return data[index]; }
		u8 const& operator[](size_t index) const { return data[index]; }
		
		size_t size() const { return data.size(); }
		
	private:
		/// Data insertion helpers
		void insert(u8);
		template <size_t N>
		void insert(std::array<u8, N>);
		void insert(assembly::Marker);
		
	private:
		utl::vector<u8> data;
	};
	
	class StreamIterator {
	public:
		explicit StreamIterator(AssemblyStream const& stream): stream(stream) {}
		
		Element next();
		template <typename T>
		T nextAs();
		
		size_t currentLine() const { return line; }
		
	private:
		void verify(Element const&, Marker) const;
		template <typename T>
		T get();
		
	private:
		AssemblyStream const& stream;
		size_t line = 1;
		size_t index = 0;
	};

	template <typename T>
	T StreamIterator::nextAs() {
		Element const elem = next();
		verify(elem, ToMarker<T>::value);
		return elem.get<T>();
	}
	
}

#endif // SCATHA_ASSEMBLY_ASSEMBLYSTREAM_H_

