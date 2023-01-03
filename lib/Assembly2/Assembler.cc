#include "Assembly2/Assembler.h"

#include <span>

#include <utl/bit.hpp>
#include <utl/hashmap.hpp>
#include <utl/scope_guard.hpp>
#include <utl/utility.hpp>

#include "Assembly2/AssemblyStream.h"
#include "Assembly2/Map.h"
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
    
    void dispatch(Instruction const& inst);
    void translate(MoveInst const&);
    void translate(JumpInst const&);
    void translate(CallInst const&);
    void translate(ReturnInst const&);
    void translate(TerminateInst const&);
    void translate(AllocaInst const&);
    void translate(CompareInst const&);
    void translate(TestInst const&);
    void translate(SetInst const&);
    void translate(ArithmeticInst const&);
    void translate(Label const&);
    
    void dispatch(Value const& value);
    void translate(RegisterIndex const&);
    void translate(MemoryAddress const&);
    void translate(Value8 const&);
    void translate(Value16 const&);
    void translate(Value32 const&);
    void translate(Value64 const&);
    
    void put(vm::OpCode o) {
        SC_ASSERT(o != OpCode::_count, "Invalid opcode.");
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
    for (auto& inst: stream) {
        dispatch(inst);
    }
    postProcess();
}

void Context::dispatch(Instruction const& inst) {
    inst.visit([this](auto& inst) { translate(inst); });
}

void Context::translate(MoveInst const& mov) {
    OpCode const opcode = mapMove(mov.dest().valueType(), mov.source().valueType());
    put(opcode);
    dispatch(mov.dest());
    dispatch(mov.source());
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

void Context::translate(ReturnInst const& ret) {
    put(OpCode::ret);
}

void Context::translate(TerminateInst const& term) {
    put(OpCode::terminate);
}

void Context::translate(AllocaInst const& alloca_) {
    put(OpCode::alloca_);
    dispatch(alloca_.dest());
    dispatch(alloca_.source());
}

void Context::translate(CompareInst const& cmp) {
    OpCode const opcode = mapCompare(cmp.type(), cmp.lhs().valueType(), cmp.rhs().valueType());
    put(opcode);
    dispatch(cmp.lhs());
    dispatch(cmp.rhs());
}

void Context::translate(TestInst const& test) {
    OpCode const opcode = mapTest(test.type());
    put(opcode);
    dispatch(test.operand().get<RegisterIndex>());
}

void Context::translate(SetInst const& set) {
    OpCode const opcode = mapSet(set.operation());
    put(opcode);
    dispatch(set.dest());
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

void Context::translate(Label const& label) {
    if (!options.startFunction.empty() && label.name() == options.startFunction) {
        program.start = currentPosition();
    }
    labels.insert({ label.id(), currentPosition() });
}

void Context::dispatch(Value const& value) {
    value.visit([this](auto& value) { translate(value); });
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
