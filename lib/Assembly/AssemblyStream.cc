#include "Assembly/AssemblyStream.h"

#include "Assembly/AssemblerIssue.h"
#include "Basic/Memory.h"

namespace scatha::assembly {

	/// MARK: AssemblyStream
	AssemblyStream& operator<<(AssemblyStream& str, Instruction i) {
		str.insert(Marker::Instruction);
		str.insert(utl::to_underlying(i));
		return str;
	}
	
	AssemblyStream& operator<<(AssemblyStream& str, Value8 v) {
		str.insert(Marker::Value8);
		str.insert(decompose(v.value));
		return str;
	}
	
	AssemblyStream& operator<<(AssemblyStream& str, Value16 v) {
		str.insert(Marker::Value16);
		str.insert(decompose(v.value));
		return str;
	}
	
	AssemblyStream& operator<<(AssemblyStream& str, Value32 v) {
		str.insert(Marker::Value32);
		str.insert(decompose(v.value));
		return str;
	}
	
	AssemblyStream& operator<<(AssemblyStream& str, Value64 v) {
		str.insert(Marker::Value64);
		str.insert(decompose(v.value));
		return str;
	}
	
	AssemblyStream& operator<<(AssemblyStream& str, RegisterIndex r) {
		str.insert(Marker::RegisterIndex);
		str.insert(r.index);
		return str;
	}
	
	AssemblyStream& operator<<(AssemblyStream& str, MemoryAddress address) {
		str.insert(Marker::MemoryAddress);
		str.insert(address.ptrRegIdx);
		str.insert(address.offset);
		str.insert(address.offsetShift);
		return str;
	}
	
	AssemblyStream& operator<<(AssemblyStream& str, Label l) {
		str.insert(Marker::Label);
		str.insert(decompose(l));
		return str;
	}
	
	void AssemblyStream::insert(u8 value) {
		data.push_back(value);
	}
	
	template <size_t N>
	void AssemblyStream::insert(std::array<u8, N> value) {
		for (auto byte: value) {
			insert(byte);
		}
	}
	
	void AssemblyStream::insert(assembly::Marker m) {
		insert(decompose(m));
	}
	
	/// MARK: StreamIterator
	Element StreamIterator::next() {
		if (index == stream.data.size()) {
			return EndOfProgram{};
		}
		Marker const marker = static_cast<Marker>(get<std::underlying_type_t<Marker>>());
		validate(marker, line);
		switch (marker) {
			case Marker::Instruction:
				++line;
				return Instruction(get<u8>());
				
			case Marker::Label:
				++line;
				return Label(get<u64>(), get<u64>());
				
			case Marker::RegisterIndex:
				return RegisterIndex(get<u8>());
				
			case Marker::MemoryAddress:
				return MemoryAddress(get<u8>(), get<u8>(), get<u8>());
			
			case Marker::Value8:
				return Value8(get<u8>());
				
			case Marker::Value16:
				return Value16(get<u16>());
				
			case Marker::Value32:
				return Value32(get<u32>());
				
			case Marker::Value64:
				return Value64(get<u64>());
				
			SC_NO_DEFAULT_CASE();
		}
	}
	
	void StreamIterator::verify(Element const& e, Marker m) const {
		if (e.marker() != m) {
			throw InvalidMarker(e.marker(), line);
		}
	}
	
	template <typename T>
	T StreamIterator::get() {
		SC_ASSERT(index + sizeof(T) <= stream.data.size(), "Out of range");
		auto const result = read<T>(&stream.data[index]);
		index += sizeof(T);
		return result;
	}
	
}
