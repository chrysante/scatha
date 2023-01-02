#include "Assembly2/Assembler.h"

#include <span>

#include <utl/bit.hpp>
#include <utl/hashmap.hpp>
#include <utl/scope_guard.hpp>
#include <utl/utility.hpp>

#include "Assembly2/Elements.h"
#include "Assembly2/AssemblyStream.h"
#include "Basic/Memory.h"
#include "VM/OpCode.h"
#include "VM/Program.h"

using namespace scatha;
using namespace asm2;

using vm::OpCode;

namespace {

struct Jumpsite {
    size_t codePosition;
    u64 targetID;
};

struct LabelPlaceholder {};

struct Context {

    explicit Context(AssemblyStream const& stream, AssemblerOptions options, vm::Program& program):
        stream(stream), options(options), program(program), instructions(program.instructions) {}
    
    void run();
    
    void dispatch(Element const& element);
    void translate(Element const& element) { SC_UNREACHABLE(); }
    void translate(MoveInst const&);
    void translate(JumpInst const&);
    void translate(CallInst const&);
    void translate(ReturnInst const&);
    void translate(TerminateInst const&);
    void translate(StoreRegAddress const&);
    void translate(CompareInst const&);
    void translate(SetInst const&);
    void translate(ArithmeticInst const&);
    void translate(Label const&);
    void translate(RegisterIndex const&);
    void translate(MemoryAddress const&);
    void translate(Value8 const&);
    void translate(Value16 const&);
    void translate(Value32 const&);
    void translate(Value64 const&);
    
    void put(vm::OpCode o) {
        program.instructions.push_back(utl::to_underlying(o));
    }
    
    template <typename T>
    void put(u64 value) {
        for (auto byte: decompose(utl::narrow_cast<T>(value))) {
            instructions.push_back(byte);
        }
    }

    void put(LabelPlaceholder) {
        /// Labels have a size of 4.
        put<u32>(~0u);
    }
    
    void registerJumpSite(size_t offsetValuePos, u64 targetID);
    
    void postProcess();
    
    size_t currentPosition() const { return instructions.size(); }
    
    AssemblyStream const& stream;
    AssemblerOptions options;
    vm::Program& program;
    utl::vector<u8>& instructions;
    // Mapping Label ID -> Code position
    utl::hashmap<u64, size_t> labels;
    // List of all code position with a jump site
    utl::vector<Jumpsite> jumpsites;
};

} // namespace

vm::Program asm2::assemble(AssemblyStream const& assemblyStream, AssemblerOptions options) {
    vm::Program program;
    Context ctx(assemblyStream, options, program);
    ctx.run();
    return program;
}

void Context::run() {
    for (auto& elem: stream) {
        dispatch(*elem);
    }
    postProcess();
}

void Context::dispatch(Element const& element) {
    visit(element, [this](auto& elem) { translate(elem); });
}

void Context::translate(MoveInst const& mov) {
    visit(mov.dest(), mov.source(), utl::overload{
        [](auto& dest, auto& src) {
            SC_DEBUGFAIL(); // No matching instruction
        },
        [&](RegisterIndex const& dest, RegisterIndex const& src) {
            put(OpCode::movRR);
        },
        [&](RegisterIndex const& dest, Value64 const& src) {
            put(OpCode::movRV);
        },
        [&](RegisterIndex const& dest, MemoryAddress const& src) {
            put(OpCode::movRM);
        },
        [&](MemoryAddress const& dest, RegisterIndex const& src) {
            put(OpCode::movMR);
        },
    });
    dispatch(mov.dest());
    dispatch(mov.source());
}

void Context::translate(JumpInst const& jmp) {
    // clang-format off
    OpCode const opcode = UTL_MAP_ENUM(jmp.condition(), OpCode, {
        { CompareOperation::None,      OpCode::jmp },
        { CompareOperation::Less,      OpCode::jl  },
        { CompareOperation::LessEq,    OpCode::jle },
        { CompareOperation::Greater,   OpCode::jg  },
        { CompareOperation::GreaterEq, OpCode::jge },
        { CompareOperation::Eq,        OpCode::je  },
        { CompareOperation::NotEq,     OpCode::jne },
    });
    // clang-format on
    put(opcode);
    registerJumpSite(currentPosition(), jmp.targetLabelID());
    put(LabelPlaceholder{});
    return;
}

void Context::translate(CallInst const& call) {
    put(OpCode::call);
    registerJumpSite(currentPosition(), call.functionLabelID());
    put(LabelPlaceholder{});
    put<u8>(call.regPtrOffset());
}

void Context::translate(ReturnInst const& ret) {
    put(OpCode::ret);
}

void Context::translate(TerminateInst const& term) {
    put(OpCode::terminate);
}

void Context::translate(StoreRegAddress const& storeRegAddr) {
    put(OpCode::storeRegAddress);
    dispatch(storeRegAddr.dest());
    dispatch(storeRegAddr.source());
}

void Context::translate(CompareInst const& cmp) {
    // clang-format off
    visit(cmp.lhs(), cmp.rhs(), utl::overload{
        [](auto& lhs, auto& rhs) {
            SC_DEBUGFAIL(); // No matching instruction
        },
        [&](RegisterIndex const& lhs, RegisterIndex const& rhs) {
            OpCode const opcode = UTL_MAP_ENUM(cmp.type(), OpCode, {
                { Type::Signed,   OpCode::icmpRR },
                { Type::Unsigned, OpCode::ucmpRR },
                { Type::Float,    OpCode::fcmpRR },
            });
            put(opcode);
        },
        [&](RegisterIndex const& lhs, Value64 const& rhs) {
            OpCode const opcode = UTL_MAP_ENUM(cmp.type(), OpCode, {
                { Type::Signed,   OpCode::icmpRV },
                { Type::Unsigned, OpCode::ucmpRV },
                { Type::Float,    OpCode::fcmpRV },
            });
            put(opcode);
        },
    });
    // clang-format on
    dispatch(cmp.lhs());
    dispatch(cmp.rhs());
}

void Context::translate(SetInst const& set) {
    // clang-format off
    OpCode const opcode = UTL_MAP_ENUM(set.operation(), OpCode, {
        { CompareOperation::None,      OpCode::_count },
        { CompareOperation::Less,      OpCode::setl  },
        { CompareOperation::LessEq,    OpCode::setle },
        { CompareOperation::Greater,   OpCode::setg  },
        { CompareOperation::GreaterEq, OpCode::setge },
        { CompareOperation::Eq,        OpCode::sete  },
        { CompareOperation::NotEq,     OpCode::setne },
    });
    // clang-format on
    put(opcode);
    dispatch(set.dest());
}

void Context::translate(ArithmeticInst const& arithmetic) {
    // clang-format off
    OpCode const opcode = visit(arithmetic.lhs(), arithmetic.rhs(), utl::overload{
        [](auto&, auto&) -> OpCode {
            SC_DEBUGFAIL(); /// No matching instruction.
        },
        [&](RegisterIndex const& lhs, RegisterIndex const& rhs) {
            switch (arithmetic.type()) {
            case Type::Signed:
                // TODO: Take care of shift and bitwise operations.
                return UTL_MAP_ENUM(arithmetic.operation(), OpCode, {
                    { ArithmeticOperation::Add,  OpCode::addRR  },
                    { ArithmeticOperation::Sub,  OpCode::subRR  },
                    { ArithmeticOperation::Mul,  OpCode::mulRR  },
                    { ArithmeticOperation::Div,  OpCode::idivRR },
                    { ArithmeticOperation::Rem,  OpCode::iremRR },
                    { ArithmeticOperation::ShL,  OpCode::_count },
                    { ArithmeticOperation::ShR,  OpCode::_count },
                    { ArithmeticOperation::And,  OpCode::_count },
                    { ArithmeticOperation::Or,   OpCode::_count },
                    { ArithmeticOperation::XOr,  OpCode::_count },
                });
            case Type::Unsigned:
                return UTL_MAP_ENUM(arithmetic.operation(), OpCode, {
                    { ArithmeticOperation::Add,  OpCode::addRR  },
                    { ArithmeticOperation::Sub,  OpCode::subRR  },
                    { ArithmeticOperation::Mul,  OpCode::mulRR  },
                    { ArithmeticOperation::Div,  OpCode::divRR  },
                    { ArithmeticOperation::Rem,  OpCode::remRR  },
                    { ArithmeticOperation::ShL,  OpCode::_count },
                    { ArithmeticOperation::ShR,  OpCode::_count },
                    { ArithmeticOperation::And,  OpCode::_count },
                    { ArithmeticOperation::Or,   OpCode::_count },
                    { ArithmeticOperation::XOr,  OpCode::_count },
                });
            case Type::Float:
                return UTL_MAP_ENUM(arithmetic.operation(), OpCode, {
                    { ArithmeticOperation::Add,  OpCode::faddRR },
                    { ArithmeticOperation::Sub,  OpCode::fsubRR },
                    { ArithmeticOperation::Mul,  OpCode::fmulRR },
                    { ArithmeticOperation::Div,  OpCode::fdivRR },
                    { ArithmeticOperation::Rem,  OpCode::_count },
                    { ArithmeticOperation::ShL,  OpCode::_count },
                    { ArithmeticOperation::ShR,  OpCode::_count },
                    { ArithmeticOperation::And,  OpCode::_count },
                    { ArithmeticOperation::Or,   OpCode::_count },
                    { ArithmeticOperation::XOr,  OpCode::_count },
                });
            case Type::_count: SC_UNREACHABLE();
            }
        },
        [&](RegisterIndex const& lhs, Value64 const& rhs) {
            switch (arithmetic.type()) {
            case Type::Signed:
                // TODO: Take care of shift and bitwise operations.
                return UTL_MAP_ENUM(arithmetic.operation(), OpCode, {
                    { ArithmeticOperation::Add,  OpCode::addRV  },
                    { ArithmeticOperation::Sub,  OpCode::subRV  },
                    { ArithmeticOperation::Mul,  OpCode::mulRV  },
                    { ArithmeticOperation::Div,  OpCode::idivRV },
                    { ArithmeticOperation::Rem,  OpCode::iremRV },
                    { ArithmeticOperation::ShL,  OpCode::_count },
                    { ArithmeticOperation::ShR,  OpCode::_count },
                    { ArithmeticOperation::And,  OpCode::_count },
                    { ArithmeticOperation::Or,   OpCode::_count },
                    { ArithmeticOperation::XOr,  OpCode::_count },
                });
            case Type::Unsigned:
                return UTL_MAP_ENUM(arithmetic.operation(), OpCode, {
                    { ArithmeticOperation::Add,  OpCode::addRV  },
                    { ArithmeticOperation::Sub,  OpCode::subRV  },
                    { ArithmeticOperation::Mul,  OpCode::mulRV  },
                    { ArithmeticOperation::Div,  OpCode::divRV  },
                    { ArithmeticOperation::Rem,  OpCode::remRV  },
                    { ArithmeticOperation::ShL,  OpCode::_count },
                    { ArithmeticOperation::ShR,  OpCode::_count },
                    { ArithmeticOperation::And,  OpCode::_count },
                    { ArithmeticOperation::Or,   OpCode::_count },
                    { ArithmeticOperation::XOr,  OpCode::_count },
                });
            case Type::Float:
                return UTL_MAP_ENUM(arithmetic.operation(), OpCode, {
                    { ArithmeticOperation::Add,  OpCode::faddRV },
                    { ArithmeticOperation::Sub,  OpCode::fsubRV },
                    { ArithmeticOperation::Mul,  OpCode::fmulRV },
                    { ArithmeticOperation::Div,  OpCode::fdivRV },
                    { ArithmeticOperation::Rem,  OpCode::_count },
                    { ArithmeticOperation::ShL,  OpCode::_count },
                    { ArithmeticOperation::ShR,  OpCode::_count },
                    { ArithmeticOperation::And,  OpCode::_count },
                    { ArithmeticOperation::Or,   OpCode::_count },
                    { ArithmeticOperation::XOr,  OpCode::_count },
                });
            case Type::_count: SC_UNREACHABLE();
            }
        },
        [&](RegisterIndex const& lhs, MemoryAddress const& rhs) {
            switch (arithmetic.type()) {
            case Type::Signed:
                // TODO: Take care of shift and bitwise operations.
                return UTL_MAP_ENUM(arithmetic.operation(), OpCode, {
                    { ArithmeticOperation::Add,  OpCode::addRM  },
                    { ArithmeticOperation::Sub,  OpCode::subRM  },
                    { ArithmeticOperation::Mul,  OpCode::mulRM  },
                    { ArithmeticOperation::Div,  OpCode::idivRM },
                    { ArithmeticOperation::Rem,  OpCode::iremRM },
                    { ArithmeticOperation::ShL,  OpCode::_count },
                    { ArithmeticOperation::ShR,  OpCode::_count },
                    { ArithmeticOperation::And,  OpCode::_count },
                    { ArithmeticOperation::Or,   OpCode::_count },
                    { ArithmeticOperation::XOr,  OpCode::_count },
                });
            case Type::Unsigned:
                return UTL_MAP_ENUM(arithmetic.operation(), OpCode, {
                    { ArithmeticOperation::Add,  OpCode::addRM  },
                    { ArithmeticOperation::Sub,  OpCode::subRM  },
                    { ArithmeticOperation::Mul,  OpCode::mulRM  },
                    { ArithmeticOperation::Div,  OpCode::divRM  },
                    { ArithmeticOperation::Rem,  OpCode::remRM  },
                    { ArithmeticOperation::ShL,  OpCode::_count },
                    { ArithmeticOperation::ShR,  OpCode::_count },
                    { ArithmeticOperation::And,  OpCode::_count },
                    { ArithmeticOperation::Or,   OpCode::_count },
                    { ArithmeticOperation::XOr,  OpCode::_count },
                });
            case Type::Float:
                return UTL_MAP_ENUM(arithmetic.operation(), OpCode, {
                    { ArithmeticOperation::Add,  OpCode::faddRM },
                    { ArithmeticOperation::Sub,  OpCode::fsubRM },
                    { ArithmeticOperation::Mul,  OpCode::fmulRM },
                    { ArithmeticOperation::Div,  OpCode::fdivRM },
                    { ArithmeticOperation::Rem,  OpCode::_count },
                    { ArithmeticOperation::ShL,  OpCode::_count },
                    { ArithmeticOperation::ShR,  OpCode::_count },
                    { ArithmeticOperation::And,  OpCode::_count },
                    { ArithmeticOperation::Or,   OpCode::_count },
                    { ArithmeticOperation::XOr,  OpCode::_count },
                });
            case Type::_count: SC_UNREACHABLE();
            }
        },
    });
    // clang-format on
    put(opcode);
    dispatch(arithmetic.lhs());
    dispatch(arithmetic.rhs());
}

void Context::translate(Label const& label) {
    labels.insert({ label.uniqueID(), currentPosition() });
}

void Context::translate(RegisterIndex const& regIdx) {
    put<u8>(regIdx.value());
}

void Context::translate(MemoryAddress const& memAddr) {
    put<u8>(memAddr.registerIndex());
    put<u8>(memAddr.offset());
    put<u8>(memAddr.offsetShift());
}

void Context::translate(Value8 const& value) {
    put<u8>(value.value());
}

void Context::translate(Value16 const& value) {
    put<u16>(value.value());
}

void Context::translate(Value32 const& value) {
    put<u32>(value.value());
}

void Context::translate(Value64 const& value) {
    put<u64>(value.value());
}

void Context::registerJumpSite(size_t offsetValuePos, u64 targetID) {
    jumpsites.push_back({
        .codePosition = offsetValuePos,
        .targetID     = targetID
    });
}

void Context::postProcess() {
    for (auto const& [position, targetID]: jumpsites) {
        auto const itr = labels.find(targetID);
        if (itr == labels.end()) {
            SC_DEBUGFAIL(); // Use of undeclared label.
        }
        size_t const targetPosition = itr->second;
        i32 const offset = utl::narrow_cast<i32>(static_cast<i64>(targetPosition) - static_cast<i64>(position));
        store(&instructions[position], offset + static_cast<i32>(sizeof(OpCode)));
    }
}
