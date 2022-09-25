#include "Assembly/AssemblyUtil.h"

#include <array>
#include <concepts>

#include <utl/math.hpp>
#include <utl/utility.hpp>

namespace scatha::assembly {
	
	namespace {
		
		struct OpCodeTable {
			template <std::same_as<vm::OpCode>... Codes>
			constexpr OpCodeTable(Instruction i, std::tuple<Marker, Marker, Codes>... elems):
				instruction(i),
				data{}
			{
				for (auto& x: data) {
					x = vm::OpCode::_count;
				}
				([&](auto&& e){
					size_t const i = utl::log2(utl::to_underlying(std::get<0>(e)));
					size_t const j = utl::log2(utl::to_underlying(std::get<1>(e)));
					data[i * matrixHeight + j] = std::get<2>(e);
				}(elems), ...);
			}
			
			constexpr vm::OpCode operator()(Element const& a, Element const& b) const {
				size_t const i = utl::log2(utl::to_underlying(a.marker()));
				size_t const j = utl::log2(utl::to_underlying(b.marker()));
				auto const result = data[i * matrixHeight + j];
				return result;
			}
			
			size_t constexpr static matrixHeight = utl::log2(utl::to_underlying(Marker::EndOfProgram));
			
			Instruction instruction;
			std::array<vm::OpCode, matrixHeight * matrixHeight> data;
		};
		
	}
	
	vm::OpCode mapInstruction(Instruction i) {
		switch (i) {
			case Instruction::allocReg:  return vm::OpCode::allocReg;
			case Instruction::setBrk:    return vm::OpCode::setBrk;
			case Instruction::call:      return vm::OpCode::call;
			case Instruction::ret:       return vm::OpCode::ret;
			case Instruction::terminate: return vm::OpCode::terminate;
				
			case Instruction::jmp:       return vm::OpCode::jmp;
			case Instruction::je:        return vm::OpCode::je;
			case Instruction::jne:       return vm::OpCode::jne;
			case Instruction::jl:        return vm::OpCode::jl;
			case Instruction::jle:       return vm::OpCode::jle;
			case Instruction::jg:        return vm::OpCode::jg;
			case Instruction::jge:       return vm::OpCode::jge;
				
			case Instruction::callExt:   return vm::OpCode::callExt;
		
			SC_NO_DEFAULT_CASE();
		}
	}
	
	vm::OpCode mapInstruction(Instruction i, Element const& arg1, Element const& arg2) {
		switch (i) {
			case Instruction::mov: {
				constexpr OpCodeTable table = {
					Instruction::mov,
					std::tuple{ Marker::RegisterIndex, Marker::RegisterIndex, vm::OpCode::movRR },
					std::tuple{ Marker::RegisterIndex, Marker::Value64,       vm::OpCode::movRV },
					std::tuple{ Marker::RegisterIndex, Marker::MemoryAddress, vm::OpCode::movRM },
					std::tuple{ Marker::MemoryAddress, Marker::RegisterIndex, vm::OpCode::movMR },
				};
				return table(arg1, arg2);
			}
			case Instruction::ucmp: {
				constexpr OpCodeTable table = {
					Instruction::ucmp,
					std::tuple{ Marker::RegisterIndex, Marker::RegisterIndex, vm::OpCode::ucmpRR },
					std::tuple{ Marker::RegisterIndex, Marker::Value64,       vm::OpCode::ucmpRV },
				};
				return table(arg1, arg2);
			}
			case Instruction::icmp: {
				constexpr OpCodeTable table = {
					Instruction::icmp,
					std::tuple{ Marker::RegisterIndex, Marker::RegisterIndex, vm::OpCode::icmpRR },
					std::tuple{ Marker::RegisterIndex, Marker::Value64,       vm::OpCode::icmpRV },
				};
				return table(arg1, arg2);
			}
			case Instruction::fcmp: {
				constexpr OpCodeTable table = {
					Instruction::fcmp,
					std::tuple{ Marker::RegisterIndex, Marker::RegisterIndex, vm::OpCode::fcmpRR },
					std::tuple{ Marker::RegisterIndex, Marker::Value64,       vm::OpCode::fcmpRV },
				};
				return table(arg1, arg2);
			}
			case Instruction::add: {
				constexpr OpCodeTable table = {
					Instruction::add,
					std::tuple{ Marker::RegisterIndex, Marker::RegisterIndex, vm::OpCode::addRR },
					std::tuple{ Marker::RegisterIndex, Marker::Value64,       vm::OpCode::addRV },
					std::tuple{ Marker::RegisterIndex, Marker::MemoryAddress, vm::OpCode::addRM },
				};
				return table(arg1, arg2);
			}
			case Instruction::sub: {
				constexpr OpCodeTable table = {
					Instruction::sub,
					std::tuple{ Marker::RegisterIndex, Marker::RegisterIndex, vm::OpCode::subRR },
					std::tuple{ Marker::RegisterIndex, Marker::Value64,       vm::OpCode::subRV },
					std::tuple{ Marker::RegisterIndex, Marker::MemoryAddress, vm::OpCode::subRM },
				};
				return table(arg1, arg2);
			}
			case Instruction::mul: {
				constexpr OpCodeTable table = {
					Instruction::mul,
					std::tuple{ Marker::RegisterIndex, Marker::RegisterIndex, vm::OpCode::mulRR },
					std::tuple{ Marker::RegisterIndex, Marker::Value64,       vm::OpCode::mulRV },
					std::tuple{ Marker::RegisterIndex, Marker::MemoryAddress, vm::OpCode::mulRM },
				};
				return table(arg1, arg2);
			}
			case Instruction::div: {
				constexpr OpCodeTable table = {
					Instruction::div,
					std::tuple{ Marker::RegisterIndex, Marker::RegisterIndex, vm::OpCode::divRR },
					std::tuple{ Marker::RegisterIndex, Marker::Value64,       vm::OpCode::divRV },
					std::tuple{ Marker::RegisterIndex, Marker::MemoryAddress, vm::OpCode::divRM },
				};
				return table(arg1, arg2);
			}
			case Instruction::idiv: {
				constexpr OpCodeTable table = {
					Instruction::idiv,
					std::tuple{ Marker::RegisterIndex, Marker::RegisterIndex, vm::OpCode::idivRR },
					std::tuple{ Marker::RegisterIndex, Marker::Value64,       vm::OpCode::idivRV },
					std::tuple{ Marker::RegisterIndex, Marker::MemoryAddress, vm::OpCode::idivRM },
				};
				return table(arg1, arg2);
			}
			case Instruction::rem: {
				constexpr OpCodeTable table = {
					Instruction::rem,
					std::tuple{ Marker::RegisterIndex, Marker::RegisterIndex, vm::OpCode::remRR },
					std::tuple{ Marker::RegisterIndex, Marker::Value64,       vm::OpCode::remRV },
					std::tuple{ Marker::RegisterIndex, Marker::MemoryAddress, vm::OpCode::remRM },
				};
				return table(arg1, arg2);
			}
			case Instruction::irem: {
				constexpr OpCodeTable table = {
					Instruction::irem,
					std::tuple{ Marker::RegisterIndex, Marker::RegisterIndex, vm::OpCode::iremRR },
					std::tuple{ Marker::RegisterIndex, Marker::Value64,       vm::OpCode::iremRV },
					std::tuple{ Marker::RegisterIndex, Marker::MemoryAddress, vm::OpCode::iremRM },
				};
				return table(arg1, arg2);
			}
			case Instruction::fadd: {
				constexpr OpCodeTable table = {
					Instruction::fadd,
					std::tuple{ Marker::RegisterIndex, Marker::RegisterIndex, vm::OpCode::faddRR },
					std::tuple{ Marker::RegisterIndex, Marker::Value64,       vm::OpCode::faddRV },
					std::tuple{ Marker::RegisterIndex, Marker::MemoryAddress, vm::OpCode::faddRM },
				};
				return table(arg1, arg2);
			}
			case Instruction::fsub: {
				constexpr OpCodeTable table = {
					Instruction::fsub,
					std::tuple{ Marker::RegisterIndex, Marker::RegisterIndex, vm::OpCode::fsubRR },
					std::tuple{ Marker::RegisterIndex, Marker::Value64,       vm::OpCode::fsubRV },
					std::tuple{ Marker::RegisterIndex, Marker::MemoryAddress, vm::OpCode::fsubRM },
				};
				return table(arg1, arg2);
			}
			case Instruction::fmul: {
				constexpr OpCodeTable table = {
					Instruction::fmul,
					std::tuple{ Marker::RegisterIndex, Marker::RegisterIndex, vm::OpCode::fmulRR },
					std::tuple{ Marker::RegisterIndex, Marker::Value64,       vm::OpCode::fmulRV },
					std::tuple{ Marker::RegisterIndex, Marker::MemoryAddress, vm::OpCode::fmulRM },
				};
				return table(arg1, arg2);
			}
			case Instruction::fdiv: {
				constexpr OpCodeTable table = {
					Instruction::fdiv,
					std::tuple{ Marker::RegisterIndex, Marker::RegisterIndex, vm::OpCode::fdivRR },
					std::tuple{ Marker::RegisterIndex, Marker::Value64,       vm::OpCode::fdivRV },
					std::tuple{ Marker::RegisterIndex, Marker::MemoryAddress, vm::OpCode::fdivRM },
				};
				return table(arg1, arg2);
			}
				
			SC_NO_DEFAULT_CASE();
		}
	}
	
}
