#include "CodeGen/CodeGenerator.h"

#include <utl/hashset.hpp>
#include <utl/utility.hpp>

#include "Assembly/Assembly.h"
#include "CodeGen/CodeGenUtil.h"

namespace scatha::codegen {

CodeGenerator::CodeGenerator(ic::ThreeAddressCode const& tac): tac(tac) {}

static assembly::Label toAsm(ic::Label const& l) {
    return assembly::Label(l.functionID.rawValue(), l.index);
}

static assembly::Label toAsm(ic::FunctionLabel const& l) {
    return assembly::Label(l.functionID().rawValue());
}

assembly::AssemblyStream CodeGenerator::run() {
    using namespace assembly;
    AssemblyStream a;
    for (size_t index = 0; index < tac.statements.size(); ++index) {
        auto const& line = tac.statements[index];
        std::visit(
            utl::visitor{ [&](ic::Label const& label) { a << toAsm(label); },
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
                if (s.arg1.is(ic::TasArgument::empty)) {
                    break;
                }
                a << Instruction::mov << resolve(s.result) << resolve(s.arg1);
                break;
            }
            case ic::Operation::param: {
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
                for (auto const& [index, offset] : currentFunction.parameterRegisterLocations()) {
                    a[index] = rd.numUsedRegisters() + 2 + offset;
                }
                currentFunction.resetParams();
                break;
            }
            case ic::Operation::ret: {
                if (!s.arg1.is(ic::TasArgument::empty)) /* if it is not a void
                                                           return statement */
                {
                    auto const argRegister = rd.resolve(s.arg1);
                    if (!argRegister || *argRegister != RegisterIndex(0)) {
                        rd.markUsed(1);
                        a << Instruction::mov << RegisterIndex(0) << resolve(s.arg1);
                    }
                }
                a << Instruction::ret;
                break;
            }
            case ic::Operation::add: [[fallthrough]];
            case ic::Operation::sub: [[fallthrough]];
            case ic::Operation::mul: [[fallthrough]];
            case ic::Operation::div: [[fallthrough]];
            case ic::Operation::idiv: [[fallthrough]];
            case ic::Operation::rem: [[fallthrough]];
            case ic::Operation::irem: [[fallthrough]];
            case ic::Operation::fadd: [[fallthrough]];
            case ic::Operation::fsub: [[fallthrough]];
            case ic::Operation::fmul: [[fallthrough]];
            case ic::Operation::fdiv: [[fallthrough]];
            case ic::Operation::sl: [[fallthrough]];
            case ic::Operation::sr: [[fallthrough]];
            case ic::Operation::And: [[fallthrough]];
            case ic::Operation::Or: [[fallthrough]];
            case ic::Operation::XOr: generateBinaryArithmetic(a, s); break;
            case ic::Operation::eq: [[fallthrough]];
            case ic::Operation::neq: [[fallthrough]];
            case ic::Operation::ils: [[fallthrough]];
            case ic::Operation::ileq: [[fallthrough]];
            case ic::Operation::ig: [[fallthrough]];
            case ic::Operation::igeq: [[fallthrough]];
            case ic::Operation::uls: [[fallthrough]];
            case ic::Operation::uleq: [[fallthrough]];
            case ic::Operation::ug: [[fallthrough]];
            case ic::Operation::ugeq: [[fallthrough]];
            case ic::Operation::feq: [[fallthrough]];
            case ic::Operation::fneq: [[fallthrough]];
            case ic::Operation::fls: [[fallthrough]];
            case ic::Operation::fleq: [[fallthrough]];
            case ic::Operation::fg: [[fallthrough]];
            case ic::Operation::fgeq: generateComparisonStore(a, s); break;
            case ic::Operation::lnt: {
                a << Instruction::mov << resolve(s.result) << resolve(s.arg1);
                a << Instruction::lnt << resolve(s.result);
                break;
            }
            case ic::Operation::bnt: {
                a << Instruction::mov << resolve(s.result) << resolve(s.arg1);
                a << Instruction::bnt << resolve(s.result);
                break;
            }
            case ic::Operation::jmp: generateJump(a, s); break;

            case ic::Operation::ifPlaceholder: [[fallthrough]];
            case ic::Operation::_count: SC_UNREACHABLE();
            }
            } },
            line);
    }

    return a;
}

void CodeGenerator::generateBinaryArithmetic(assembly::AssemblyStream& a, ic::ThreeAddressStatement const& s) {
    a << assembly::Instruction::mov << resolve(s.result) << resolve(s.arg1);
    a << mapOperation(s.operation) << resolve(s.result) << resolve(s.arg2);
}

void CodeGenerator::generateComparison(assembly::AssemblyStream& a, ic::ThreeAddressStatement const& s) {
    auto const cmp = mapComparison(s.operation);
    if (s.arg1.is(ic::TasArgument::literalValue)) {
        assembly::RegisterIndex const tmp = rd.makeTemporary();
        a << assembly::Instruction::mov << tmp << resolve(s.arg1);
        a << cmp << tmp << resolve(s.arg2);
    } else {
        a << cmp << resolve(s.arg1) << resolve(s.arg2);
    }
}

void CodeGenerator::generateComparisonStore(assembly::AssemblyStream& a, ic::ThreeAddressStatement const& s) {
    generateComparison(a, s);
    a << mapComparisonStore(s.operation) << resolve(s.result);
}

void CodeGenerator::generateJump(assembly::AssemblyStream& a, ic::ThreeAddressStatement const& s) {
    using namespace assembly;
    a << Instruction::jmp << toAsm(s.getLabel());
}

void CodeGenerator::generateConditionalJump(assembly::AssemblyStream& a,
                                            ic::ThreeAddressStatement const& ifStatement,
                                            ic::ThreeAddressStatement const& jumpStatement) {
    SC_ASSERT(isJump(jumpStatement.operation), "which must be a jump");
    if (ifStatement.operation == ic::Operation::ifPlaceholder) {
        a << assembly::Instruction::utest << resolve(ifStatement.arg1);
    } else {
        SC_ASSERT(ic::isRelop(ifStatement.operation), "operation must be if placeholder or a relop");
        generateComparison(a, ifStatement);
    }
    a << mapConditionalJump(ifStatement.operation) << toAsm(jumpStatement.getLabel());
}

void CodeGenerator::ResolvedArg::streamInsert(assembly::AssemblyStream& str) const {
    arg.visit(utl::visitor{ [&](ic::EmptyArgument const&) { SC_DEBUGBREAK(); },
                            [&](ic::Variable const& var) { str << self.rd.resolve(var); },
                            [&](ic::Temporary const& tmp) { str << self.rd.resolve(tmp); },
                            [&](ic::LiteralValue const& lit) { str << assembly::Value64(lit.value); },
                            [&](ic::Label const& label) { str << toAsm(label); },
                            [&](ic::If) { SC_DEBUGFAIL(); } });
}

assembly::AssemblyStream& operator<<(assembly::AssemblyStream& str, CodeGenerator::ResolvedArg a) {
    a.streamInsert(str);
    return str;
}

CodeGenerator::ResolvedArg CodeGenerator::resolve(ic::TasArgument const& arg) {
    return { *this, arg };
}

} // namespace scatha::codegen
