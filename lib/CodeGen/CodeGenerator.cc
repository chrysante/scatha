#include "CodeGen/CodeGenerator.h"

#include <utl/utility.hpp>

#include "Assembly/Assembly.h"

namespace scatha::codegen {
	
	CodeGenerator::CodeGenerator(ic::ThreeAddressCode const& tac):
		tac(tac)
	{
		
	}

	assembly::Label toAsm(ic::Label const& l) {
		return assembly::Label(l.functionID.id(), l.index);
	}
	
	assembly::Label toAsm(ic::FunctionLabel const& l) {
		return assembly::Label(l.functionID().id(), 0);
	}
	
	void CodeGenerator::submit(assembly::AssemblyStream& a, ic::TasArgument const& arg) {
		arg.visit(utl::visitor{
			[&](ic::EmptyArgument const&) {
				SC_DEBUGBREAK();
			},
			[&](ic::Variable const& var) {
				a << rd.resolve(var);
			},
			[&](ic::Temporary const& tmp) {
				a << rd.resolve(tmp);
			},
			[&](ic::LiteralValue const& lit) {
				a << assembly::Value64(lit.value);
			},
			[&](ic::Label const& label) {
				a << toAsm(label);
			}
		});
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
				},
				[&](ic::ThreeAddressStatement const& s) {
					switch (s.operation) {
						case ic::Operation::mov: {
							a << Instruction::mov;
							submit(a, s.result);
							submit(a, s.arg1);
							break;
						}
						case ic::Operation::pushParam: {
		
							break;
						}
						case ic::Operation::getResult: {
		
							break;
						}
						case ic::Operation::call: {
		
							break;
						}
						case ic::Operation::ret: {
		
							break;
						}
						case ic::Operation::add: {
		
							break;
						}
						case ic::Operation::sub: {
		
							break;
						}
						case ic::Operation::mul: {
		
							break;
						}
						case ic::Operation::div: {
		
							break;
						}
						case ic::Operation::idiv: {
		
							break;
						}
						case ic::Operation::rem: {
		
							break;
						}
						case ic::Operation::irem: {
		
							break;
						}
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
	
}
