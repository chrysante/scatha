#include "AssemblyStream.h"

namespace scatha::assembly {
	
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
	
}
