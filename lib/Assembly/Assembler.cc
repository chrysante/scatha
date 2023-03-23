#include "Assembly/Assembler.h"

#include <span>

#include <svm/OpCode.h>
#include <svm/Program.h>
#include <utl/bit.hpp>
#include <utl/hashmap.hpp>
#include <utl/scope_guard.hpp>
#include <utl/utility.hpp>

#include "Assembly/AssemblyStream.h"
#include "Assembly/Block.h"
#include "Assembly/Instruction.h"
#include "Assembly/Map.h"
#include "Assembly/Value.h"
#include "Basic/Memory.h"

using namespace scatha;
using namespace Asm;

using svm::OpCode;

namespace {

struct Jumpsite {
    size_t codePosition;
    u64 targetID;
};

struct LabelPlaceholder {};

struct Context {

    explicit Context(AssemblyStream const& stream, AssemblerOptions options):
        stream(stream), options(options) {}

    void run();

    void dispatch(Instruction const& inst);
    void translate(MoveInst const&);
    void translate(CMoveInst const&);
    void translate(JumpInst const&);
    void translate(CallInst const&);
    void translate(CallExtInst const&);
    void translate(ReturnInst const&);
    void translate(TerminateInst const&);
    void translate(AllocaInst const&);
    void translate(CompareInst const&);
    void translate(TestInst const&);
    void translate(SetInst const&);
    void translate(UnaryArithmeticInst const&);
    void translate(ArithmeticInst const&);

    void dispatch(Value const& value);
    void translate(RegisterIndex const&);
    void translate(MemoryAddress const&);
    void translate(Value8 const&);
    void translate(Value16 const&);
    void translate(Value32 const&);
    void translate(Value64 const&);

    void put(svm::OpCode o) {
        SC_ASSERT(o != OpCode::_count, "Invalid opcode.");
        instructions.push_back(utl::to_underlying(o));
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
    utl::vector<u8> instructions;
    u64 start = 0;

    /// Maps Label ID to Code position
    utl::hashmap<u64, size_t> labels;
    /// List of all code position with a jump site
    utl::vector<Jumpsite> jumpsites;
};

} // namespace

utl::vector<u8> Asm::assemble(AssemblyStream const& assemblyStream,
                              AssemblerOptions options) {
    Context ctx(assemblyStream, options);
    ctx.run();
    svm::ProgramHeader const header{
        .versionString = {},
        .size          = sizeof(svm::ProgramHeader) +
                ctx.instructions.size(), // + data.size()
        .dataOffset = sizeof(svm::ProgramHeader),
        .textOffset = sizeof(svm::ProgramHeader),
        .start      = ctx.start
    };
    utl::vector<u8> program(sizeof(svm::ProgramHeader) + header.size);
    std::memcpy(program.data(), &header, sizeof(header));
    std::memcpy(program.data() + sizeof(header),
                ctx.instructions.data(),
                ctx.instructions.size());
    return program;
}

void Context::run() {
    for (auto& block: stream) {
        if (!options.startFunction.empty() &&
            block.name() == options.startFunction)
        {
            start = currentPosition();
        }
        labels.insert({ block.id(), currentPosition() });
        for (auto& inst: block) {
            dispatch(inst);
        }
    }
    postProcess();
}

void Context::dispatch(Instruction const& inst) {
    inst.visit([this](auto& inst) { translate(inst); });
}

void Context::translate(MoveInst const& mov) {
    auto const [opcode, size] = mapMove(mov.dest().valueType(),
                                        mov.source().valueType(),
                                        mov.numBytes());
    put(opcode);
    dispatch(mov.dest());
    dispatch(promote(mov.source(), size));
}

void Context::translate(CMoveInst const& cmov) {
    auto const [opcode, size] = mapCMove(cmov.condition(),
                                         cmov.dest().valueType(),
                                         cmov.source().valueType(),
                                         cmov.numBytes());
    put(opcode);
    dispatch(cmov.dest());
    dispatch(promote(cmov.source(), size));
}

void Context::translate(JumpInst const& jmp) {
    OpCode const opcode = mapJump(jmp.condition());
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

void Context::translate(CallExtInst const& call) {
    put(OpCode::callExt);
    put<u8>(call.regPtrOffset());
    put<u8>(call.slot());
    put<u16>(call.index());
}

void Context::translate(ReturnInst const& ret) { put(OpCode::ret); }

void Context::translate(TerminateInst const& term) { put(OpCode::terminate); }

void Context::translate(AllocaInst const& alloca_) {
    put(OpCode::alloca_);
    dispatch(alloca_.dest());
    dispatch(alloca_.source());
}

void Context::translate(CompareInst const& cmp) {
    OpCode const opcode = mapCompare(cmp.type(),
                                     promote(cmp.lhs().valueType(), 8),
                                     promote(cmp.rhs().valueType(), 8));
    put(opcode);
    dispatch(promote(cmp.lhs(), 8));
    dispatch(promote(cmp.rhs(), 8));
}

void Context::translate(TestInst const& test) {
    OpCode const opcode = mapTest(test.type());
    put(opcode);
    SC_ASSERT(test.operand().is<RegisterIndex>(),
              "Can only test values in registers");
    dispatch(test.operand().get<RegisterIndex>());
}

void Context::translate(SetInst const& set) {
    OpCode const opcode = mapSet(set.operation());
    put(opcode);
    dispatch(set.dest());
}

void Context::translate(UnaryArithmeticInst const& inst) {
    switch (inst.operation()) {
    case UnaryArithmeticOperation::LogicalNot:
        put(OpCode::lnt);
        break;
    case UnaryArithmeticOperation::BitwiseNot:
        put(OpCode::bnt);
        break;
    default:
        SC_UNREACHABLE();
    }
    translate(inst.operand());
}

void Context::translate(ArithmeticInst const& inst) {
    OpCode const opcode = mapArithmetic(inst.operation(),
                                        inst.type(),
                                        inst.dest().valueType(),
                                        inst.source().valueType());
    put(opcode);
    dispatch(inst.dest());
    dispatch(inst.source());
}

void Context::dispatch(Value const& value) {
    value.visit([this](auto& value) { translate(value); });
}

void Context::translate(RegisterIndex const& regIdx) {
    put<u8>(regIdx.value());
}

void Context::translate(MemoryAddress const& memAddr) {
    put<u8>(memAddr.baseptrRegisterIndex());
    put<u8>(memAddr.offsetCountRegisterIndex());
    put<u8>(memAddr.constantOffsetMultiplier());
    put<u8>(memAddr.constantInnerOffset());
}

void Context::translate(Value8 const& value) { put<u8>(value.value()); }

void Context::translate(Value16 const& value) { put<u16>(value.value()); }

void Context::translate(Value32 const& value) { put<u32>(value.value()); }

void Context::translate(Value64 const& value) { put<u64>(value.value()); }

void Context::registerJumpSite(size_t offsetValuePos, u64 targetID) {
    jumpsites.push_back(
        { .codePosition = offsetValuePos, .targetID = targetID });
}

void Context::postProcess() {
    for (auto const& [position, targetID]: jumpsites) {
        auto const itr = labels.find(targetID);
        if (itr == labels.end()) {
            SC_DEBUGFAIL(); // Use of undeclared label.
        }
        size_t const targetPosition = itr->second;
        i32 const offset            = utl::narrow_cast<i32>(
            static_cast<i64>(targetPosition) - static_cast<i64>(position));
        store(&instructions[position],
              offset + static_cast<i32>(sizeof(OpCode)));
    }
}
