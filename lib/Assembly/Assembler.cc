#include "Assembly/Assembler.h"

#include <array>
#include <ostream>
#include <iostream>
#include <span>

#include <utl/bit.hpp>
#include <utl/hashmap.hpp>
#include <utl/utility.hpp>
#include <utl/scope_guard.hpp>

#include "Basic/Memory.h"
#include "Assembly/AssemblerIssue.h"

#include "VM/OpCode.h"

namespace scatha::assembly {
	
	using namespace vm;
	
	Program Assembler::assemble() {
		program = nullptr;
		index = 0;
		labels.clear();
		jumpsites.clear();
		line = 1;
		
		Program result;
		program = &result;
		
		for (Element elem = eat(); elem.marker() != Marker::EndOfProgram; ++line, elem = eat()) {
			switch (elem.marker()) {
				case Marker::Instruction: {
					auto const instruction = elem.get<Instruction>();
					currentInstruction = instruction;
					processInstruction(instruction);
					break;
				}
				case Marker::Label:
					registerLabel(elem.get<Label>());
					break;
					
				default:
					throw UnexpectedElement(elem, line);
			}
		}
		
		postProcess();
		
		return result;
	}
	
	void Assembler::processInstruction(Instruction i) {
		switch (i) {
				using enum Instruction;
			case allocReg: {
				put(OpCode::allocReg);
				put(eatAs<Value8>());
				return;
			}
			case setBrk: {
				put(OpCode::setBrk);
				put(eatAs<Value8>());
				return;
			}
			case call: {
				registerJumpsite();
				put(OpCode::call);
				put(LabelPlaceholder{});
				put(eatAs<Value8>());
				return;
			}
			case ret: {
				put(OpCode::ret);
				return;
			}
			case terminate: {
				put(OpCode::terminate);
				return;
			}
			case mov: {
				Element arg0 = eat();
				Element arg1 = eat();
				switch (arg0.marker()) {
					case Marker::RegisterIndex:
						switch (arg1.marker()) {
							case Marker::RegisterIndex:
								put(OpCode::movRR);
								put(arg0.get<RegisterIndex>());
								put(arg1.get<RegisterIndex>());
								break;
							case Marker::Value64:
								put(OpCode::movRV);
								put(arg0.get<RegisterIndex>());
								put(arg1.get<Value64>());
								break;
							case Marker::MemoryAddress:
								put(OpCode::movRM);
								put(arg0.get<RegisterIndex>());
								put(arg1.get<MemoryAddress>());
								break;
							default:
								throw UnexpectedElement(arg1, line);
						}
						break;
					case Marker::MemoryAddress:
						if (arg1.marker() != Marker::RegisterIndex) {
							throw UnexpectedElement(arg1, line);
						}
						put(OpCode::movMR);
						put(arg0.get<MemoryAddress>());
						put(arg1.get<RegisterIndex>());
						break;
					default:
						throw UnexpectedElement(arg0, line);
				}
				return;
			}
			case jmp:
				registerJumpsite();
				put(OpCode::jmp);
				put(LabelPlaceholder{});
				return;
			case je:
				registerJumpsite();
				put(OpCode::je);
				put(LabelPlaceholder{});
				return;
			case jne:
				registerJumpsite();
				put(OpCode::jne);
				put(LabelPlaceholder{});
				return;
			case jl:
				registerJumpsite();
				put(OpCode::jl);
				put(LabelPlaceholder{});
				return;
			case jle:
				registerJumpsite();
				put(OpCode::jle);
				put(LabelPlaceholder{});
				return;
			case jg:
				registerJumpsite();
				put(OpCode::jg);
				put(LabelPlaceholder{});
				return;
			case jge: {
				registerJumpsite();
				put(OpCode::jge);
				put(LabelPlaceholder{});
				return;
			}
			case ucmp: {
				process_RR_RV(OpCode::ucmpRR, OpCode::ucmpRV);
				return;
			}
			case icmp: {
				process_RR_RV(OpCode::icmpRR, OpCode::icmpRV);
				return;
			}
			case fcmp: {
				process_RR_RV(OpCode::fcmpRR, OpCode::fcmpRV);
				return;
			}
			case add: {
				process_RR_RV_RM(OpCode::addRR, OpCode::addRV, OpCode::addRM);
				return;
			}
			case sub: {
				process_RR_RV_RM(OpCode::subRR, OpCode::subRV, OpCode::subRM);
				return;
			}
			case mul: {
				process_RR_RV_RM(OpCode::mulRR, OpCode::mulRV, OpCode::mulRM);
				return;
			}
			case div: {
				process_RR_RV_RM(OpCode::divRR, OpCode::divRV, OpCode::divRM);
				return;
			}
			case rem: {
				process_RR_RV_RM(OpCode::remRR, OpCode::remRV, OpCode::remRM);
				return;
			}
			case fadd: {
				process_RR_RV_RM(OpCode::faddRR, OpCode::faddRV, OpCode::faddRM);
				return;
			}
			case fsub: {
				process_RR_RV_RM(OpCode::fsubRR, OpCode::fsubRV, OpCode::fsubRM);
				return;
			}
			case fmul: {
				process_RR_RV_RM(OpCode::fmulRR, OpCode::fmulRV, OpCode::fmulRM);
				return;
			}
			case fdiv: {
				process_RR_RV_RM(OpCode::fdivRR, OpCode::fdivRV, OpCode::fdivRM);
				return;
			}
			case callExt: {
				put(OpCode::callExt);
				put(eatAs<Value8>());
				put(eatAs<Value8>());
				put(eatAs<Value16>());
				return;
			}
			SC_NO_DEFAULT_CASE();
		}
	}
	
	void Assembler::process_RR_RV(vm::OpCode RR, vm::OpCode RV) {
		Element arg0 = eat();
		Element arg1 = eat();
		
		if (arg0.marker() != Marker::RegisterIndex) {
			throw UnexpectedElement(arg0, line);
		}
		
		auto const r = arg0.get<RegisterIndex>();

		switch (arg1.marker()) {
			case Marker::RegisterIndex:
				put(RR);
				put(r);
				put(arg1.get<RegisterIndex>());
				return;
			case Marker::Value64:
				put(RV);
				put(r);
				put(arg1.get<Value64>());
				return;
			
			default:
				throw UnexpectedElement(arg1, line);
		}
	}
	
	void Assembler::process_RR_RV_RM(vm::OpCode RR, vm::OpCode RV, vm::OpCode RM) {
		Element arg0 = eat();
		Element arg1 = eat();
		
		if (arg0.marker() != Marker::RegisterIndex) {
			throw UnexpectedElement(arg0, line);
		}
		
		auto const r = arg0.get<RegisterIndex>();

		switch (arg1.marker()) {
			case Marker::RegisterIndex:
				put(RR);
				put(r);
				put(arg1.get<RegisterIndex>());
				return;
			case Marker::Value64:
				put(RV);
				put(r);
				put(arg1.get<Value64>());
				return;
			case Marker::MemoryAddress:
				put(RM);
				put(r);
				put(arg1.get<MemoryAddress>());
				return;
			
			default:
				throw UnexpectedElement(arg1, line);
		}
	}
	
	void Assembler::registerLabel(Label label) {
		labels.insert({ label, program->instructions.size() });
	}
	
	void Assembler::registerJumpsite() {
		jumpsites.push_back({ program->instructions.size(), line, eatAs<Label>() });
	}
	
	void Assembler::postProcess() {
		for (auto const& [position, line, label]: jumpsites) {
			auto const itr = labels.find(label);
			if (itr == labels.end()) {
				throw UseOfUndeclaredLabel(label, line);
			}
			size_t const target = itr->second;
			SC_ASSERT(position >= 1, "Position must be positive");
			auto const instruction = read<OpCode>(&program->instructions[position]);
			SC_ASSERT(isJump(instruction), "Before a label should be a jump or call statement at this stage.");
			store(&program->instructions[position + 1], i32(i64(target) - i64(position)));
		}
	}

	Element Assembler::eat() {
		using namespace assembly;
		if (index == data.size()) {
			return EndOfProgram{};
		}
		Marker const marker = static_cast<Marker>(eat<std::underlying_type_t<Marker>>());
		validate(marker, line);
		switch (marker) {
			case Marker::Instruction:
				return Instruction(eat<u8>());
				
			case Marker::Label:
				return Label(eat<u64>());
				
			case Marker::RegisterIndex:
				return RegisterIndex(eat<u8>());
				
			case Marker::MemoryAddress:
				return MemoryAddress(eat<u8>(), eat<u8>(), eat<u8>());
			
			case Marker::Value8:
				return Value8(eat<u8>());
				
			case Marker::Value16:
				return Value16(eat<u16>());
				
			case Marker::Value32:
				return Value32(eat<u32>());
				
			case Marker::Value64:
				return Value64(eat<u64>());
				
			SC_NO_DEFAULT_CASE();
		}
	}
	
	template <typename T>
	T Assembler::eatAs() {
		Element const elem = eat();
		if (elem.marker() != ToMarker<T>::value) {
			throw InvalidMarker(elem.marker(), line);
		}
		return elem.get<T>();
	}
	
	template <typename T>
	T Assembler::eat() {
		SC_ASSERT(index + sizeof(T) <= data.size(), "Out of range");
		T result;
		std::memcpy(&result, &data[index], sizeof(T));
		index += sizeof(T);
		return result;
	}
	
	void Assembler::put(vm::OpCode o) {
		program->instructions.push_back(utl::to_underlying(o));
	}
	
	void Assembler::put(LabelPlaceholder) {
		for (int i = 0; i < 4; ++i) {
			program->instructions.push_back(0);
		}
	}
	
	void Assembler::put(RegisterIndex r) {
		program->instructions.push_back(r.index);
	}
	
	void Assembler::put(MemoryAddress m) {
		program->instructions.push_back(m.ptrRegIdx);
		program->instructions.push_back(m.offset);
		program->instructions.push_back(m.offsetShift);
	}
	
	void Assembler::put(Value8 v) {
		for (auto byte: decompose(v.value)) {
			program->instructions.push_back(byte);
		}
	}
	
	void Assembler::put(Value16 v) {
		for (auto byte: decompose(v.value)) {
			program->instructions.push_back(byte);
		}
	}
	
	void Assembler::put(Value32 v) {
		for (auto byte: decompose(v.value)) {
			program->instructions.push_back(byte);
		}
	}
	
	void Assembler::put(Value64 v) {
		for (auto byte: decompose(v.value)) {
			program->instructions.push_back(byte);
		}
	}
	
	// Input stream operators
	Assembler& operator<<(Assembler& a, Instruction i) {
		a.insert(Marker::Instruction);
		a.insert(utl::to_underlying(i));
		return a;
	}
	
	Assembler& operator<<(Assembler& a, Value8 v) {
		a.insert(Marker::Value8);
		a.insert(decompose(v.value));
		return a;
	}
	
	Assembler& operator<<(Assembler& a, Value16 v) {
		a.insert(Marker::Value16);
		a.insert(decompose(v.value));
		return a;
	}
	
	Assembler& operator<<(Assembler& a, Value32 v) {
		a.insert(Marker::Value32);
		a.insert(decompose(v.value));
		return a;
	}
	
	Assembler& operator<<(Assembler& a, Value64 v) {
		a.insert(Marker::Value64);
		a.insert(decompose(v.value));
		return a;
	}
	
	Assembler& operator<<(Assembler& a, RegisterIndex r) {
		a.insert(Marker::RegisterIndex);
		a.insert(r.index);
		return a;
	}
	
	Assembler& operator<<(Assembler& a, MemoryAddress address) {
		a.insert(Marker::MemoryAddress);
		a.insert(address.ptrRegIdx);
		a.insert(address.offset);
		a.insert(address.offsetShift);
		return a;
	}
	
	Assembler& operator<<(Assembler& a, Label l) {
		a.insert(Marker::Label);
		a.insert(decompose(l.id));
		return a;
	}
	
}
