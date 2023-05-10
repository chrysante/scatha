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
#include "Common/Memory.h"

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
    explicit Context(AssemblyStream const& stream,
                     std::unordered_map<std::string, size_t>& sym):
        stream(stream), sym(sym) {}

    void run();

    void dispatch(Instruction const& inst);
    void translate(MoveInst const&);
    void translate(CMoveInst const&);
    void translate(JumpInst const&);
    void translate(CallInst const&);
    void translate(CallExtInst const&);
    void translate(ReturnInst const&);
    void translate(TerminateInst const&);
    void translate(LIncSPInst const&);
    void translate(LEAInst const&);
    void translate(LDAInst const&);
    void translate(CompareInst const&);
    void translate(TestInst const&);
    void translate(SetInst const&);
    void translate(UnaryArithmeticInst const&);
    void translate(ArithmeticInst const&);
    void translate(TruncExtInst const&);
    void translate(ConvertInst const&);

    void dispatch(Value const& value);
    void translate(RegisterIndex const&);
    void translate(MemoryAddress const&);
    void translate(Value8 const&);
    void translate(Value16 const&);
    void translate(Value32 const&);
    void translate(Value64 const&);

    void put(svm::OpCode o) {
        SC_ASSERT(o != OpCode::_count, "Invalid opcode.");
        put<std::underlying_type_t<svm::OpCode>>(utl::to_underlying(o));
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
    std::unordered_map<std::string, size_t>& sym;
    utl::vector<u8> instructions;

    /// Maps Label ID to Code position
    utl::hashmap<u64, size_t> labels;
    /// List of all code position with a jump site
    utl::vector<Jumpsite> jumpsites;
    /// Address of the `start` or `main` function.
    size_t startAddress = 0;
};

} // namespace

AssemblerResult Asm::assemble(AssemblyStream const& astr) {
    AssemblerResult result;
    Context ctx(astr, result.symbolTable);
    ctx.run();
    std::span<u8 const> dataSection = astr.dataSection();
    svm::ProgramHeader const header{
        .versionString = {},
        .size          = sizeof(svm::ProgramHeader) + dataSection.size() +
                ctx.instructions.size(),
        .dataOffset   = sizeof(svm::ProgramHeader),
        .textOffset   = sizeof(svm::ProgramHeader) + dataSection.size(),
        .startAddress = ctx.startAddress
    };
    result.program.resize(sizeof(svm::ProgramHeader) + header.size);
    std::memcpy(result.program.data(), &header, sizeof(header));
    std::memcpy(result.program.data() + header.dataOffset,
                astr.dataSection().data(),
                astr.dataSection().size());
    std::memcpy(result.program.data() + header.textOffset,
                ctx.instructions.data(),
                ctx.instructions.size());
    return result;
}

void Context::run() {
    for (auto& block: stream) {
        if (block.isPublic()) {
            sym.insert({ std::string(block.name()), currentPosition() });
            if (block.name().starts_with("main")) {
                startAddress = currentPosition();
            }
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

void Context::translate(LIncSPInst const& lincsp) {
    put(OpCode::lincsp);
    dispatch(lincsp.dest());
    dispatch(lincsp.offset());
}

void Context::translate(LEAInst const& lea) {
    put(OpCode::lea);
    dispatch(lea.dest());
    dispatch(lea.address());
}

void Context::translate(LDAInst const& lda) {
    put(OpCode::lda);
    dispatch(lda.dest());
    dispatch(lda.offset());
}

void Context::translate(CompareInst const& cmp) {
    OpCode const opcode = mapCompare(cmp.type(),
                                     promote(cmp.lhs().valueType(), 8),
                                     promote(cmp.rhs().valueType(), 8),
                                     cmp.width());
    put(opcode);
    dispatch(promote(cmp.lhs(), 8));
    dispatch(promote(cmp.rhs(), 8));
}

void Context::translate(TestInst const& test) {
    OpCode const opcode = mapTest(test.type(), test.width());
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
    OpCode const opcode = inst.width() == 4 ?
                              mapArithmetic32(inst.operation(),
                                              inst.dest().valueType(),
                                              inst.source().valueType()) :
                              mapArithmetic64(inst.operation(),
                                              inst.dest().valueType(),
                                              inst.source().valueType());
    put(opcode);
    dispatch(inst.dest());
    dispatch(inst.source());
}

void Context::translate(TruncExtInst const& inst) {
    auto const opcode = [&] {
        if (inst.type() == Type::Signed) {
            switch (inst.fromBits()) {
            case 1:
                return OpCode::sext1;
            case 8:
                return OpCode::sext8;
            case 16:
                return OpCode::sext16;
            case 32:
                return OpCode::sext32;
            default:
                SC_UNREACHABLE();
            }
        }
        else {
            SC_ASSERT(inst.type() == Type::Float, "");
            switch (inst.fromBits()) {
            case 32:
                return OpCode::fext;
            case 64:
                return OpCode::ftrunc;
            default:
                SC_UNREACHABLE();
            }
        }
    }();
    put(opcode);
    dispatch(inst.operand());
}

void Context::translate(ConvertInst const& inst) {
    auto const opcode = [&] {
#define MAP_CONV(FromType, FromBits, ToType, ToBits)                           \
    if (inst.fromType() == Type::FromType && inst.fromBits() == FromBits &&    \
        inst.toType() == Type::ToType && inst.toBits() == ToBits)
        using enum svm::OpCode;
        MAP_CONV(Signed, 8, Float, 32) { return s8tof32; }
        MAP_CONV(Signed, 16, Float, 32) { return s16tof32; }
        MAP_CONV(Signed, 32, Float, 32) { return s32tof32; }
        MAP_CONV(Signed, 64, Float, 32) { return s64tof32; }
        MAP_CONV(Unsigned, 8, Float, 32) { return u8tof32; }
        MAP_CONV(Unsigned, 16, Float, 32) { return u16tof32; }
        MAP_CONV(Unsigned, 32, Float, 32) { return u32tof32; }
        MAP_CONV(Unsigned, 64, Float, 32) { return u64tof32; }
        MAP_CONV(Signed, 8, Float, 64) { return s8tof64; }
        MAP_CONV(Signed, 16, Float, 64) { return s16tof64; }
        MAP_CONV(Signed, 32, Float, 64) { return s32tof64; }
        MAP_CONV(Signed, 64, Float, 64) { return s64tof64; }
        MAP_CONV(Unsigned, 8, Float, 64) { return u8tof64; }
        MAP_CONV(Unsigned, 16, Float, 64) { return u16tof64; }
        MAP_CONV(Unsigned, 32, Float, 64) { return u32tof64; }
        MAP_CONV(Unsigned, 64, Float, 64) { return u64tof64; }

        MAP_CONV(Float, 32, Signed, 8) { return f32tos8; }
        MAP_CONV(Float, 32, Signed, 16) { return f32tos16; }
        MAP_CONV(Float, 32, Signed, 32) { return f32tos32; }
        MAP_CONV(Float, 32, Signed, 64) { return f32tos64; }
        MAP_CONV(Float, 32, Unsigned, 8) { return f32tou8; }
        MAP_CONV(Float, 32, Unsigned, 16) { return f32tou16; }
        MAP_CONV(Float, 32, Unsigned, 32) { return f32tou32; }
        MAP_CONV(Float, 32, Unsigned, 64) { return f32tou64; }
        MAP_CONV(Float, 64, Signed, 8) { return f64tos8; }
        MAP_CONV(Float, 64, Signed, 16) { return f64tos16; }
        MAP_CONV(Float, 64, Signed, 32) { return f64tos32; }
        MAP_CONV(Float, 64, Signed, 64) { return f64tos64; }
        MAP_CONV(Float, 64, Unsigned, 8) { return f64tou8; }
        MAP_CONV(Float, 64, Unsigned, 16) { return f64tou16; }
        MAP_CONV(Float, 64, Unsigned, 32) { return f64tou32; }
        MAP_CONV(Float, 64, Unsigned, 64) { return f64tou64; }
#undef MAP_CONV

        SC_UNREACHABLE();
    }();
    put(opcode);
    dispatch(inst.operand());
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
