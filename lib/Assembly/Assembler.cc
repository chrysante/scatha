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
#include "Assembly/AssemblyUtil.h"

#include "VM/OpCode.h"

namespace scatha::assembly {
	
	using namespace vm;
	
	Assembler::Assembler(AssemblyStream const& str):
		stream(str)
	{
		
	}
	
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
			case allocReg:
				put(OpCode::allocReg);
				put(eatAs<Value8>());
				return;
			
			case setBrk:
				put(OpCode::setBrk);
				put(eatAs<RegisterIndex>());
				return;
			
			case call:
				registerJumpsite();
				put(OpCode::call);
				put(LabelPlaceholder{});
				put(eatAs<Value8>());
				return;
			
			case ret:
				put(OpCode::ret);
				return;
			
			case terminate:
				put(OpCode::terminate);
				return;
			
			case callExt:
				put(OpCode::callExt);
				put(eatAs<Value8>());
				put(eatAs<Value8>());
				put(eatAs<Value16>());
				return;
				
			case mov:  [[fallthrough]];
			case ucmp: [[fallthrough]];
			case icmp: [[fallthrough]];
			case fcmp: [[fallthrough]];
			case add:  [[fallthrough]];
			case sub:  [[fallthrough]];
			case mul:  [[fallthrough]];
			case div:  [[fallthrough]];
			case idiv: [[fallthrough]];
			case rem:  [[fallthrough]];
			case irem: [[fallthrough]];
			case fadd: [[fallthrough]];
			case fsub: [[fallthrough]];
			case fmul: [[fallthrough]];
			case fdiv:
				processBinaryInstruction(i);
				return;
					
			case jmp: [[fallthrough]];
			case je:  [[fallthrough]];
			case jne: [[fallthrough]];
			case jl:  [[fallthrough]];
			case jle: [[fallthrough]];
			case jg:  [[fallthrough]];
			case jge:
				processJump(i);
				return;
				
			SC_NO_DEFAULT_CASE();
		}
	}
	
	void Assembler::processBinaryInstruction(Instruction i) {
		auto const arg1 = eat();
		auto const arg2 = eat();
		auto const opcode = mapInstruction(i, arg1, arg2);
		if (opcode == OpCode::_count) {
			throw InvalidArguments(i, arg1, arg2, line);
		}
		put(opcode);
		put(arg1);
		put(arg2);
	}
	
	void Assembler::processJump(Instruction i) {
		registerJumpsite();
		put(mapInstruction(i));
		put(LabelPlaceholder{});
		return;
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

	/// MARK: eat()
	Element Assembler::eat() {
		using namespace assembly;
		if (index == stream.data.size()) {
			return EndOfProgram{};
		}
		Marker const marker = static_cast<Marker>(eatImpl<std::underlying_type_t<Marker>>());
		validate(marker, line);
		switch (marker) {
			case Marker::Instruction:
				return Instruction(eatImpl<u8>());
				
			case Marker::Label:
				return Label(eatImpl<u64>(), eatImpl<u64>());
				
			case Marker::RegisterIndex:
				return RegisterIndex(eatImpl<u8>());
				
			case Marker::MemoryAddress:
				return MemoryAddress(eatImpl<u8>(), eatImpl<u8>(), eatImpl<u8>());
			
			case Marker::Value8:
				return Value8(eatImpl<u8>());
				
			case Marker::Value16:
				return Value16(eatImpl<u16>());
				
			case Marker::Value32:
				return Value32(eatImpl<u32>());
				
			case Marker::Value64:
				return Value64(eatImpl<u64>());
				
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
	T Assembler::eatImpl() {
		SC_ASSERT(index + sizeof(T) <= stream.data.size(), "Out of range");
		auto const result = read<T>(&stream.data[index]);
		index += sizeof(T);
		return result;
	}
	
	/// MARK: put()
	void Assembler::put(vm::OpCode o) {
		program->instructions.push_back(utl::to_underlying(o));
	}
	
	void Assembler::put(LabelPlaceholder) {
		for (int i = 0; i < 4; ++i) {
			program->instructions.push_back(0);
		}
	}
	
	void Assembler::put(Element const& elem) {
		std::visit([this](auto const& x) { put(x); }, elem);
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
	
}
