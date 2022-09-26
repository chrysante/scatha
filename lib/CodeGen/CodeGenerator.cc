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
		for (size_t index = 0; index < tac.statements.size(); ++index) {
			auto const& line = tac.statements[index];
			std::visit(utl::visitor{
				[&](ic::Label const& label) {
					a << toAsm(label);
				},
				[&](ic::FunctionLabel const& label) {
					a << toAsm(label);
					SC_ASSERT(rd.empty(), "rd has not been cleared");
					rd.declareParameters(label);
					a << Instruction::allocReg;
					a << Value8(-1);
					currentFunction.allocRegArgIndex = a.size() - Value8::size();
				},
				[&](ic::FunctionEndLabel) {
					a[currentFunction.allocRegArgIndex] = rd.numUsedRegisters();
					if (currentFunction.calledAnyFunction()) {
						a[currentFunction.allocRegArgIndex] += 2 + currentFunction.maxParamCount();
					}
					rd.clear();
					currentFunction = {};
				},
				[&](ic::ThreeAddressStatement const& s) {
					if (s.result.is(ic::TasArgument::conditional)) {
						// handle conditional statements separately
						SC_ASSERT(index + 1 < tac.statements.size(), "we must have another statement");
						ic::ThreeAddressStatement const& jumpStatement = tac.statements[++index].asTas();
						generateConditionalJump(a, s, jumpStatement);
						return;
					}
					
					switch (s.operation) {
						case ic::Operation::mov: {
							a << Instruction::mov << resolve(s.result) << resolve(s.arg1);
							break;
						}
						case ic::Operation::pushParam: {
							a << Instruction::mov << RegisterIndex(-1);
							currentFunction.addParam(a.size() - RegisterIndex::size(), currentFunction.paramCount());
							a << resolve(s.arg1);
							break;
						}
						case ic::Operation::getResult: {
							size_t const resultLocation = rd.numUsedRegisters() + 2;
							a << Instruction::mov << resolve(s.result);
							a << RegisterIndex(resultLocation);
							break;
						}
						case ic::Operation::call: {
							a << Instruction::call << toAsm(s.getLabel()) << Value8(rd.numUsedRegisters() + 2);
							for (auto const& [index, offset]: currentFunction.parameterRegisterLocations()) {
								a[index] = rd.numUsedRegisters() + 2 + offset;
							}
							currentFunction.resetParams();
							break;
						}
						case ic::Operation::ret: {
							if (!s.arg1.is(ic::TasArgument::empty)) /* if it is not a void return statement */ {
								auto const argRegister = rd.resolve(s.arg1);
								if (!argRegister || *argRegister != RegisterIndex(0)) {
									rd.markUsed(1);
									a << Instruction::mov << RegisterIndex(0) << resolve(s.arg1);
								}
							}
							a << Instruction::ret;
							break;
						}
						case ic::Operation::add:
						case ic::Operation::sub:
						case ic::Operation::mul:
						case ic::Operation::div:
						case ic::Operation::idiv:
						case ic::Operation::rem:
						case ic::Operation::irem:
						case ic::Operation::fadd:
						case ic::Operation::fsub:
						case ic::Operation::fmul:
						case ic::Operation::fdiv:
							generateBinaryArithmetic(a, s);
							break;
						case ic::Operation::eq:
						case ic::Operation::neq:
						case ic::Operation::ils:
						case ic::Operation::ileq:
						case ic::Operation::ig:
						case ic::Operation::igeq:
						case ic::Operation::uls:
						case ic::Operation::uleq:
						case ic::Operation::ug:
						case ic::Operation::ugeq:
						case ic::Operation::feq:
						case ic::Operation::fneq:
						case ic::Operation::fls:
						case ic::Operation::fleq:
						case ic::Operation::fg:
						case ic::Operation::fgeq:
							generateComparisonStore(a, s);
							break;
						case ic::Operation::lnt: {
							a << Instruction::mov << resolve(s.result) << resolve(s.arg1);
							a << Instruction::lnt << resolve(s.arg1);
							break;
						}
						case ic::Operation::bnt: {
							SC_DEBUGBREAK();
							break;
						}
						case ic::Operation::jmp:
							generateJump(a, s);
							break;
							
						case ic::Operation::ifPlaceholder:
						case ic::Operation::_count:
							SC_DEBUGFAIL();
					}
				}
			}, line);
		}
		
		return a;
	}
	
	void CodeGenerator::generateBinaryArithmetic(assembly::AssemblyStream& a, ic::ThreeAddressStatement const& s) {
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
				case ic::Operation::fadd: return Instruction::fadd;
				case ic::Operation::fsub: return Instruction::fsub;
				case ic::Operation::fmul: return Instruction::fmul;
				case ic::Operation::fdiv: return Instruction::fdiv;
				SC_NO_DEFAULT_CASE();
			}
		}();
		a << Instruction::mov << resolve(s.result) << resolve(s.arg1);
		a << mappedInstruction << resolve(s.result) << resolve(s.arg2);
	}
	
	void CodeGenerator::generateComparison(assembly::AssemblyStream& a, ic::ThreeAddressStatement const& s) {
		using namespace assembly;
		
		Instruction const cmp = [&]{
			switch (s.operation) {
				case ic::Operation::eq:
				case ic::Operation::neq:
				case ic::Operation::ils:
				case ic::Operation::ileq:
				case ic::Operation::ig:
				case ic::Operation::igeq:
					return Instruction::icmp;
				case ic::Operation::uls:
				case ic::Operation::uleq:
				case ic::Operation::ug:
				case ic::Operation::ugeq:
					return Instruction::ucmp;
				case ic::Operation::feq:
				case ic::Operation::fneq:
				case ic::Operation::fls:
				case ic::Operation::fleq:
				case ic::Operation::fg:
				case ic::Operation::fgeq:
					return Instruction::fcmp;
				SC_NO_DEFAULT_CASE();
			}
		}();
		
		if (s.arg1.is(ic::TasArgument::literalValue)) {
			RegisterIndex const tmp = rd.makeTemporary();
			a << Instruction::mov << tmp << resolve(s.arg1);
			a << cmp << tmp << resolve(s.arg2);
		}
		else {
			a << cmp << resolve(s.arg1) << resolve(s.arg2);
		}
	}
	
	void CodeGenerator::generateComparisonStore(assembly::AssemblyStream& a, ic::ThreeAddressStatement const& s) {
		using namespace assembly;
		generateComparison(a, s);
		
		Instruction const setInstruction = [&]{
			switch (s.operation) {
				case ic::Operation::eq:
				case ic::Operation::feq:
					return Instruction::sete;
				case ic::Operation::neq:
				case ic::Operation::fneq:
					return Instruction::setne;
				case ic::Operation::ils:
				case ic::Operation::uls:
				case ic::Operation::fls:
					return Instruction::setl;
				case ic::Operation::ileq:
				case ic::Operation::uleq:
				case ic::Operation::fleq:
					return Instruction::setle;
				case ic::Operation::ig:
				case ic::Operation::ug:
				case ic::Operation::fg:
					return Instruction::setg;
				case ic::Operation::igeq:
				case ic::Operation::ugeq:
				case ic::Operation::fgeq:
					return Instruction::setge;
					
				SC_NO_DEFAULT_CASE();
			}
		}();
		
		a << setInstruction << resolve(s.result);
	}
	
	void CodeGenerator::generateJump(assembly::AssemblyStream& a, ic::ThreeAddressStatement const& s) {
		using namespace assembly;
		a << Instruction::jmp << toAsm(s.getLabel());
	}
	
	void CodeGenerator::generateConditionalJump(assembly::AssemblyStream& a,
												ic::ThreeAddressStatement const& ifStatement,
												ic::ThreeAddressStatement const& jumpStatement)
	{
		SC_ASSERT(isJump(jumpStatement.operation), "which must be a jump");
		
		using namespace assembly;
		
		if (ifStatement.operation == ic::Operation::ifPlaceholder) {
			a << Instruction::utest << resolve(ifStatement.arg1);
		}
		else {
			SC_ASSERT(ic::isRelop(ifStatement.operation), "operation must be if placeholder or a relop");
			generateComparison(a, ifStatement);
		}
		
		Instruction const jmpInstruction = [&]{
			switch (ifStatement.operation) {
				case ic::Operation::eq:
				case ic::Operation::feq:
					return Instruction::je;
				case ic::Operation::neq:
				case ic::Operation::fneq:
					return Instruction::jne;
				case ic::Operation::ils:
				case ic::Operation::uls:
				case ic::Operation::fls:
					return Instruction::jl;
				case ic::Operation::ileq:
				case ic::Operation::uleq:
				case ic::Operation::fleq:
					return Instruction::jle;
				case ic::Operation::ig:
				case ic::Operation::ug:
				case ic::Operation::fg:
					return Instruction::jg;
				case ic::Operation::igeq:
				case ic::Operation::ugeq:
				case ic::Operation::fgeq:
					return Instruction::jge;
					
				case ic::Operation::ifPlaceholder:
					return Instruction::je;
					
				SC_NO_DEFAULT_CASE();
			}
		}();
		
		a << jmpInstruction << toAsm(jumpStatement.getLabel());
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
			},
			[&](ic::If) { SC_DEBUGFAIL(); }
		});
	}
	
	assembly::AssemblyStream& operator<<(assembly::AssemblyStream& str, CodeGenerator::ResolvedArg a) {
		a.streamInsert(str);
		return str;
	}

	CodeGenerator::ResolvedArg CodeGenerator::resolve(ic::TasArgument const& arg) {
		return { *this, arg };
	}
	
}
