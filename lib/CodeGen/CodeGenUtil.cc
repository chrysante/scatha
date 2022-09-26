#include "CodeGen/CodeGenUtil.h"

namespace scatha::codegen {

	assembly::Instruction mapOperation(ic::Operation op) {
		using namespace assembly;
		switch (op) {
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
	}

	assembly::Instruction mapComparison(ic::Operation op) {
		using namespace assembly;
		switch (op) {
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
	}

	assembly::Instruction mapComparisonStore(ic::Operation op) {
		using namespace assembly;
		switch (op) {
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
	}
	
	assembly::Instruction mapConditionalJump(ic::Operation op) {
		using namespace assembly;
		switch (op) {
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
	}
	
}
