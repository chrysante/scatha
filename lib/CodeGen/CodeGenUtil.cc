#include "CodeGen/CodeGenUtil.h"

namespace scatha::codegen {

assembly::Instruction mapOperation(ic::Operation op) {
    using namespace assembly;
    switch (op) {
    case ic::Operation::add: return Instruction::add;
    case ic::Operation::sub: return Instruction::sub;
    case ic::Operation::mul: return Instruction::mul;
    case ic::Operation::div: return Instruction::div;
    case ic::Operation::idiv: return Instruction::idiv;
    case ic::Operation::rem: return Instruction::rem;
    case ic::Operation::irem: return Instruction::irem;
    case ic::Operation::fadd: return Instruction::fadd;
    case ic::Operation::fsub: return Instruction::fsub;
    case ic::Operation::fmul: return Instruction::fmul;
    case ic::Operation::fdiv: return Instruction::fdiv;
    case ic::Operation::sl: return Instruction::sl;
    case ic::Operation::sr: return Instruction::sr;
    case ic::Operation::And: return Instruction::And;
    case ic::Operation::Or: return Instruction::Or;
    case ic::Operation::XOr: return Instruction::XOr;
    default: SC_UNREACHABLE();
    }
}

assembly::Instruction mapComparison(ic::Operation op) {
    using namespace assembly;
    switch (op) {
    case ic::Operation::eq: [[fallthrough]];
    case ic::Operation::neq: [[fallthrough]];
    case ic::Operation::ils: [[fallthrough]];
    case ic::Operation::ileq: [[fallthrough]];
    case ic::Operation::ig: [[fallthrough]];
    case ic::Operation::igeq: return Instruction::icmp;
    case ic::Operation::uls: [[fallthrough]];
    case ic::Operation::uleq: [[fallthrough]];
    case ic::Operation::ug: [[fallthrough]];
    case ic::Operation::ugeq: return Instruction::ucmp;
    case ic::Operation::feq: [[fallthrough]];
    case ic::Operation::fneq: [[fallthrough]];
    case ic::Operation::fls: [[fallthrough]];
    case ic::Operation::fleq: [[fallthrough]];
    case ic::Operation::fg: [[fallthrough]];
    case ic::Operation::fgeq: return Instruction::fcmp;
    default: SC_UNREACHABLE();
    }
}

assembly::Instruction mapComparisonStore(ic::Operation op) {
    using namespace assembly;
    switch (op) {
    case ic::Operation::eq: [[fallthrough]];
    case ic::Operation::feq: return Instruction::sete;
    case ic::Operation::neq: [[fallthrough]];
    case ic::Operation::fneq: return Instruction::setne;
    case ic::Operation::ils: [[fallthrough]];
    case ic::Operation::uls: [[fallthrough]];
    case ic::Operation::fls: return Instruction::setl;
    case ic::Operation::ileq: [[fallthrough]];
    case ic::Operation::uleq: [[fallthrough]];
    case ic::Operation::fleq: return Instruction::setle;
    case ic::Operation::ig: [[fallthrough]];
    case ic::Operation::ug: [[fallthrough]];
    case ic::Operation::fg: return Instruction::setg;
    case ic::Operation::igeq: [[fallthrough]];
    case ic::Operation::ugeq: [[fallthrough]];
    case ic::Operation::fgeq: return Instruction::setge;
    default: SC_UNREACHABLE();
    }
}

assembly::Instruction mapConditionalJump(ic::Operation op) {
    using namespace assembly;
    switch (op) {
    case ic::Operation::eq: [[fallthrough]];
    case ic::Operation::feq: return Instruction::je;
    case ic::Operation::neq: [[fallthrough]];
    case ic::Operation::fneq: return Instruction::jne;
    case ic::Operation::ils: [[fallthrough]];
    case ic::Operation::uls: [[fallthrough]];
    case ic::Operation::fls: return Instruction::jl;
    case ic::Operation::ileq: [[fallthrough]];
    case ic::Operation::uleq: [[fallthrough]];
    case ic::Operation::fleq: return Instruction::jle;
    case ic::Operation::ig: [[fallthrough]];
    case ic::Operation::ug: [[fallthrough]];
    case ic::Operation::fg: return Instruction::jg;
    case ic::Operation::igeq: [[fallthrough]];
    case ic::Operation::ugeq: [[fallthrough]];
    case ic::Operation::fgeq: return Instruction::jge;
    case ic::Operation::ifPlaceholder: return Instruction::je;
    default: SC_UNREACHABLE();
    }
}

} // namespace scatha::codegen
