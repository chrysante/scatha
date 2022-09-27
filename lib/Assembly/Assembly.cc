#include "Assembly/Assembly.h"

#include <ostream>
#include <bit>

#include <utl/utility.hpp>

#include "Assembly/AssemblerIssue.h"

namespace scatha::assembly {

	std::ostream& operator<<(std::ostream& str, Instruction i) {
		using enum Instruction;
		return str << UTL_SERIALIZE_ENUM(i, {
			{ allocReg,  "allocReg" },
			{ setBrk,    "setBrk" },
			{ call,      "call" },
			{ ret,       "ret" },
			{ terminate, "terminate" },
			{ mov,       "mov" },
			{ jmp,       "jmp" },
			{ je,        "je" },
			{ jne,       "jne" },
			{ jl,        "jl" },
			{ jle,       "jle" },
			{ jg,        "jg" },
			{ jge,       "jge" },
			{ ucmp,      "ucmp" },
			{ icmp,      "icmp" },
			{ fcmp,      "fcmp" },
			{ itest,     "itest" },
			{ utest,     "utest" },
			{ sete,      "sete" },
			{ setne,     "setne" },
			{ setl,      "setl" },
			{ setle,     "setle" },
			{ setg,      "setg" },
			{ setge,     "setge" },
			{ lnt,       "lnt" },
			{ bnt,       "bnt" },
			{ add,       "add" },
			{ sub,       "sub" },
			{ mul,       "mul" },
			{ div,       "div" },
			{ idiv,      "idiv" },
			{ rem,       "rem" },
			{ irem,      "irem" },
			{ fadd,      "fadd" },
			{ fsub,      "fsub" },
			{ fmul,      "fmul" },
			{ fdiv,      "fdiv" },
			{ sl,        "sl" },
			{ sr,        "sr" },
			{ callExt,   "callExt" },
		});
	}

	void validate(Marker m_, size_t line) {
		auto const m = utl::to_underlying(m_);
		if (m >= utl::to_underlying(Marker::EndOfProgram)) {
			throw InvalidMarker(m_, line);
		}
		if (!std::has_single_bit(m)) {
			throw InvalidMarker(m_, line);
		}
	}
	
	std::ostream& operator<<(std::ostream& str, Label l) {
		return str << "LABEL: " << l.index << std::endl;
	}
	
	std::ostream& operator<<(std::ostream& str, RegisterIndex r) {
		return str << "R[" << +r.index << "]";
	}
	
	std::ostream& operator<<(std::ostream& str, MemoryAddress address) {
		return str << "MEMORY[R[" << +address.ptrRegIdx << "] + " << +address.offset << " * (1 << " << +address.offsetShift << ")]";
	}
	
	std::ostream& operator<<(std::ostream& str, Value8 v) {
		return str << +v.value;
	}
	
	std::ostream& operator<<(std::ostream& str, Value16 v) {
		return str << v.value;
	}
	
	std::ostream& operator<<(std::ostream& str, Value32 v) {
		return str << v.value;
	}
	
	std::ostream& operator<<(std::ostream& str, Value64 v) {
		switch (v.type) {
			case Value64::UnsignedIntegral:
				return str << v.value;
			case Value64::SignedIntegral:
				return str << static_cast<i64>(v.value);
			case Value64::FloatingPoint:
				return str << utl::bit_cast<f64>(v.value);
		}
	}
	
	std::ostream& operator<<(std::ostream& str, Marker m) {
		switch (m) {
			case Marker::Instruction:
				return str << "Instruction";
			case Marker::Label:
				return str << "Label";
			case Marker::RegisterIndex:
				return str << "RegisterIndex";
			case Marker::MemoryAddress:
				return str << "MemoryAddress";
			case Marker::Value8:
				return str << "Value8";
			case Marker::Value16:
				return str << "Value16";
			case Marker::Value32:
				return str << "Value32";
			case Marker::Value64:
				return str << "Value64";
			case Marker::EndOfProgram:
				return str << "EndOfProgram";
			case Marker::none:
				SC_DEBUGFAIL();
		}
	}
	
	std::ostream& operator<<(std::ostream& str, Element const& e) {
		return std::visit([&](auto const& x) -> decltype(auto) {
			return str << x;
		}, e);
	}

	
}
