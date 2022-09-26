#include "Assembly/AssemblyUtil.h"

#include <array>
#include <concepts>
#include <iostream>
#include <ostream>

#include <utl/math.hpp>
#include <utl/utility.hpp>

#include "Assembly/Assembler.h"
#include "Assembly/AssemblerIssue.h"

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
	
	vm::OpCode mapUnaryInstruction(Instruction i) {
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
				
			case Instruction::itest:     return vm::OpCode::itest;
			case Instruction::utest:     return vm::OpCode::utest;
				
			case Instruction::sete:      return vm::OpCode::sete;
			case Instruction::setne:     return vm::OpCode::setne;
			case Instruction::setl:      return vm::OpCode::setl;
			case Instruction::setle:     return vm::OpCode::setle;
			case Instruction::setg:      return vm::OpCode::setg;
			case Instruction::setge:     return vm::OpCode::setge;
				
			case Instruction::lnt:     return vm::OpCode::lnt;
			case Instruction::bnt:     return vm::OpCode::bnt;
				
			case Instruction::callExt:   return vm::OpCode::callExt;
			
			SC_NO_DEFAULT_CASE();
		}
	}
	
	vm::OpCode mapBinaryInstruction(Instruction i, Element const& arg1, Element const& arg2) {
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
			case Instruction::lnt:
				return vm::OpCode::lnt;
			case Instruction::bnt:
				return vm::OpCode::bnt;
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
	
	using namespace vm;
	
	namespace {
		
		struct Printer {
			std::ostream& str;
			StreamIterator itr;
			sema::SymbolTable const* sym;
			
			Printer(AssemblyStream const& a, std::ostream& str, sema::SymbolTable const* sym = nullptr):
				str(str),
				itr(a),
				sym(sym)
			{}
			
			void print() {
				for (Element elem = itr.next(); elem.marker() != Marker::EndOfProgram; elem = itr.next()) {
					switch (elem.marker()) {
						case Marker::Instruction: {
							auto const instruction = elem.get<Instruction>();
							printInstruction(instruction);
							break;
						}
						case Marker::Label: {
							auto const label = elem.get<Label>();
							if (label.index >= 0) {
								str << "  ";
							}
							printLabel(label);
							str << ":";
							break;
						}
						default:
							throw UnexpectedElement(elem, itr.currentLine());
					}
					str << "\n";
				}
			}
			
			void printInstruction(Instruction i) {
				str << "    ";
				switch (i) {
						using enum Instruction;
					case allocReg:
						str << allocReg << " " << itr.nextAs<Value8>();
						return;
						
					case setBrk:
						str << setBrk << " " << itr.nextAs<RegisterIndex>();
						return;
						
					case call:
						str << call << " ";
						printLabel(itr.nextAs<Label>());
						str << ", " << itr.nextAs<Value8>();
						return;
						
					case ret:
						str << ret;
						return;
						
					case terminate:
						str << terminate;
						return;
						
					case callExt:
						str << callExt << " " << itr.nextAs<Value8>() <<
						", " << itr.nextAs<Value8>() << ", " << itr.nextAs<Value16>();
						return;
						
					case itest: [[fallthrough]];
					case utest: [[fallthrough]];
					case sete:  [[fallthrough]];
					case setne: [[fallthrough]];
					case setl:  [[fallthrough]];
					case setle: [[fallthrough]];
					case setg:  [[fallthrough]];
					case setge: [[fallthrough]];
					case lnt:   [[fallthrough]];
					case bnt:
						printUnaryInstruction(i);
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
						printBinaryInstruction(i);
						return;
						
					case jmp: [[fallthrough]];
					case je:  [[fallthrough]];
					case jne: [[fallthrough]];
					case jl:  [[fallthrough]];
					case jle: [[fallthrough]];
					case jg:  [[fallthrough]];
					case jge:
						printJump(i);
						return;
						
					case _count:
						SC_DEBUGFAIL();
				}
			}
			
			void printUnaryInstruction(Instruction i) {
				auto const arg1 = itr.next();
				str << i << " " << arg1;
			}
			
			void printBinaryInstruction(Instruction i) {
				auto const arg1 = itr.next();
				auto const arg2 = itr.next();
				str << i << " " << arg1 << ", " << arg2;
			}
			
			void printJump(Instruction i) {
				auto const label = itr.nextAs<Label>();
				str << i << " ";
				printLabel(label);
			}
			
			void printLabel(Label label) {
				if (sym != nullptr) {
					str << sym->getFunction(sema::SymbolID(label.functionID, sema::SymbolCategory::Function)).name();
					if (label.index >= 0) {
						str << ".L" << label.index;
					}
					else if (label.index == -2) {
						str << ".END";
					}
				}
				else {
					str << ".L" << label.functionID;
					if (label.index >= 0) {
						str << ":" << label.index;
					}
				}
			}
		};
		
	} // namespace
		
	void print(AssemblyStream const& a) {
		print(a, std::cout);
	}
	
	void print(AssemblyStream const& a, std::ostream& str) {
		Printer p(a, str);
		p.print();
	}
	
	void print(AssemblyStream const& a, sema::SymbolTable const& sym) {
		print(a, sym, std::cout);
	}
	
	void print(AssemblyStream const& a, sema::SymbolTable const& sym, std::ostream& str) {
		Printer p(a, str, &sym);
		p.print();
	}
	
}
