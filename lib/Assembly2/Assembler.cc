#include "Assembly2/Assembler.h"

#include <span>

#include <utl/bit.hpp>
#include <utl/hashmap.hpp>
#include <utl/scope_guard.hpp>
#include <utl/utility.hpp>

#include "Assembly2/Elements.h"
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
    
    void dispatch(Element const& element);
    void translate(Element const& element) { SC_UNREACHABLE(); }
    void translate(MoveInst const&);
    void translate(JumpInst const&);
    void translate(CallInst const&);
    void translate(ReturnInst const&);
    void translate(TerminateInst const&);
    void translate(StoreRegAddress const&);
    void translate(CompareInst const&);
    void translate(TestInst const&);
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
    for (auto& elem: stream) {
        dispatch(*elem);
    }
    postProcess();
}

void Context::dispatch(Element const& element) {
    visit(element, [this](auto& elem) { translate(elem); });
}

void Context::translate(MoveInst const& mov) {
    OpCode const opcode = mapMove(mov.dest().elementType(), mov.source().elementType());
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

void Context::translate(StoreRegAddress const& storeRegAddr) {
    put(OpCode::storeRegAddress);
    dispatch(storeRegAddr.dest());
    dispatch(storeRegAddr.source());
}

void Context::translate(CompareInst const& cmp) {
    OpCode const opcode = mapCompare(cmp.type(), cmp.lhs().elementType(), cmp.rhs().elementType());
    put(opcode);
    dispatch(cmp.lhs());
    dispatch(cmp.rhs());
}

void Context::translate(TestInst const& test) {
    OpCode const opcode = mapTest(test.type());
    put(opcode);
    dispatch(cast<RegisterIndex const&>(test.operand()));
}

void Context::translate(SetInst const& set) {
    OpCode const opcode = mapSet(set.operation());
    put(opcode);
    dispatch(set.dest());
}

void Context::translate(ArithmeticInst const& inst) {
    OpCode const opcode = mapArithmetic(inst.operation(),
                                        inst.type(),
                                        inst.lhs().elementType(),
                                        inst.rhs().elementType());
    put(opcode);
    dispatch(inst.lhs());
    dispatch(inst.rhs());
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
