#include "CodeGen/CodeGenerator.h"

#include <utl/hashset.hpp>
#include <utl/utility.hpp>

#include "Assembly/Assembly.h"

namespace scatha::codegen {
	
	CodeGenerator::CodeGenerator(ic::ThreeAddressCode const& tac):
		tac(tac)
	{
		
	}

	static assembly::Label toAsm(ic::Label const& l) {
		return assembly::Label(l.functionID.id(), l.index);
	}
	
	static assembly::Label toAsm(ic::FunctionLabel const& l) {
		return assembly::Label(l.functionID().id());
	}
	
	assembly::AssemblyStream CodeGenerator::run() {
		using namespace assembly;
		AssemblyStream a;
		for (auto const& line: tac.statements) {
			std::visit(utl::visitor{
				[&](ic::Label const& label) {
					a << toAsm(label);
				},
				[&](ic::FunctionLabel const& label) {
					a << toAsm(label);
					rd.clear();
					rd.declareParameters(label);
					size_t const numRequiredRegisters = countRegisters(&line - tac.statements.data()); // stupid hack to get the current index
					a << Instruction::allocReg << Value8(numRequiredRegisters);
				},
				[&](ic::ThreeAddressStatement const& s) {
					switch (s.operation) {
						case ic::Operation::mov: {
							a << Instruction::mov << resolve(s.result) << resolve(s.arg1);
							break;
						}
						case ic::Operation::pushParam: {
							a << Instruction::mov << RegisterIndex(rd.currentIndex() + 2 + paramIndex) << resolve(s.arg1);
							++paramIndex;
							break;
						}
						case ic::Operation::getResult: {
							a << Instruction::mov << resolve(s.result);
							a << RegisterIndex(rd.currentIndex() + 2);
							break;
						}
						case ic::Operation::call: {
							
							break;
						}
						case ic::Operation::ret: {
							if (!s.arg1.is(ic::TasArgument::empty)) /* if it is not a void return statement */ {
								auto const argRegister = rd.resolve(s.arg1);
								if (!argRegister || *argRegister != RegisterIndex(0)) {
									a << Instruction::mov << RegisterIndex(0) << resolve(s.arg1);
								}
							}
							a << Instruction::ret;
							break;
						}
						case ic::Operation::add:  [[fallthrough]];
						case ic::Operation::sub:  [[fallthrough]];
						case ic::Operation::mul:  [[fallthrough]];
						case ic::Operation::div:  [[fallthrough]];
						case ic::Operation::idiv: [[fallthrough]];
						case ic::Operation::rem:  [[fallthrough]];
						case ic::Operation::irem:
							generateBinaryExpression(a, s);
							break;
						case ic::Operation::fadd: {
		
							break;
						}
						case ic::Operation::fsub: {
		
							break;
						}
						case ic::Operation::fmul: {
		
							break;
						}
						case ic::Operation::fdiv: {
		
							break;
						}
						case ic::Operation::eq: {
		
							break;
						}
						case ic::Operation::neq: {
		
							break;
						}
						case ic::Operation::ls: {
		
							break;
						}
						case ic::Operation::leq: {
		
							break;
						}
						case ic::Operation::feq: {
		
							break;
						}
						case ic::Operation::fneq: {
		
							break;
						}
						case ic::Operation::fls: {
		
							break;
						}
						case ic::Operation::fleq: {
		
							break;
						}
						case ic::Operation::lnt: {
		
							break;
						}
						case ic::Operation::bnt: {
		
							break;
						}
						case ic::Operation::jmp: {
		
							break;
						}
						case ic::Operation::cjmp: {
		
							break;
						}
						case ic::Operation::_count:
							SC_DEBUGFAIL();
					}
				}
			}, line);
			


		}
		
		return a;
	}
	
	void CodeGenerator::generateBinaryExpression(assembly::AssemblyStream& a, ic::ThreeAddressStatement const& s) {
		using namespace assembly;
		Instruction const mappedInstruction = [&]{
			switch (s.operation) {
				case ic::Operation::add:  return Instruction::add;
				case ic::Operation::sub:  return Instruction::sub;
				case ic::Operation::mul:  return Instruction::mul;
				case ic::Operation::div:  return Instruction::div;
				case ic::Operation::idiv: return Instruction::idiv;
				case ic::Operation::rem:  return Instruction::rem;
				case ic::Operation::irem: return Instruction::irem;
					
				SC_NO_DEFAULT_CASE();
			}
		}();
		a << Instruction::mov << resolve(s.result) << resolve(s.arg1);
		a << mappedInstruction << resolve(s.result) << resolve(s.arg2);
	}
	
	void CodeGenerator::ResolvedArg::streamInsert(assembly::AssemblyStream& str) const {
		arg.visit(utl::visitor{
			[&](ic::EmptyArgument const&) {
				SC_DEBUGBREAK();
			},
			[&](ic::Variable const& var) {
				str << self.rd.resolve(var);
			},
			[&](ic::Temporary const& tmp) {
				str << self.rd.resolve(tmp);
			},
			[&](ic::LiteralValue const& lit) {
				str << assembly::Value64(lit.value);
			},
			[&](ic::Label const& label) {
				str << toAsm(label);
			}
		});
	}
	
	assembly::AssemblyStream& operator<<(assembly::AssemblyStream& str, CodeGenerator::ResolvedArg a) {
		a.streamInsert(str);
		return str;
	}

	CodeGenerator::ResolvedArg CodeGenerator::resolve(ic::TasArgument const& arg) {
		return { *this, arg };
	}

	size_t CodeGenerator::countRegisters(size_t index) const {
		++index;
		size_t result = 0;
		utl::hashset<sema::SymbolID> variables;
		utl::hashset<size_t> temporaries;
		for (; index < tac.statements.size() && tac.statements[index].index() != 2; ++index) {
			auto& s = tac.statements[index];
			std::visit(utl::visitor{
				[&](ic::ThreeAddressStatement const& s) {
					auto count = [&](ic::TasArgument const& arg) {
						arg.visit(utl::visitor{
							[&](ic::Variable const& var) {
								if (!variables.contains(var.id())) {
									variables.insert(var.id());
									++result;
								}
							},
							[&](ic::Temporary const& tmp) {
								if (!temporaries.contains(tmp.index)) {
									temporaries.insert(tmp.index);
									++result;
								}
							},
							[](auto&&) {}
						});
					};
					count(s.result);
					count(s.arg1);
					count(s.arg2);
					if (s.operation == ic::Operation::ret && s.arg1.is(ic::TasArgument::literalValue)) {
						result = std::max(result, size_t{ 1 });
					}
				},
				[](auto&&) {}
			}, s);
		}
		
		
		
		return result;
	}
	
}
