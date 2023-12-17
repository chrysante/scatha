#include "Assembly/Assembler.h"

#include <span>

#include <range/v3/view.hpp>
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
#include "Common/DebugInfo.h"

using namespace scatha;
using namespace Asm;

using svm::OpCode;

/// 4 bytes will always suffice to store code addresses unless we have binaries
/// greater than 4GB in size.
static constexpr size_t StaticAddressWidth = 4;

/// We store dynamic addresses with 8 bytes to conveniently fill one register.
static constexpr size_t DynamicAddressWidth = 8;

namespace {

struct LabelPlaceholder {};

struct AsmContext {
    explicit AsmContext(AssemblyStream const& stream,
                        std::unordered_map<std::string, size_t>& sym):
        stream(stream), sym(sym), jumpsites(stream.jumpSites()) {}

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
    void translate(LabelPosition const&);

    void put(svm::OpCode o) {
        SC_ASSERT(o != OpCode::_count, "Invalid opcode.");
        put<std::underlying_type_t<svm::OpCode>>(utl::to_underlying(o));
    }

    template <typename T>
    void put(u64 value) {
        for (auto byte: decompose(utl::narrow_cast<T>(value))) {
            binary.push_back(byte);
        }
    }

    /// Emit a placeholder for a label address and register a jumpsite for
    /// postprocessing
    void put(LabelPlaceholder, LabelID targetID, size_t width) {
        registerJumpSite(currentPosition(), targetID, width);
        for (size_t i = 0; i < width; ++i) {
            binary.push_back(0xFF);
        }
    }

    /// Used when emitting a jump or call instruction. Stores the code position
    /// of the destination and the target ID.
    void registerJumpSite(size_t offsetValuePos,
                          LabelID targetID,
                          size_t width) {
        jumpsites.push_back({
            .codePosition = offsetValuePos,
            .targetID = targetID,
            .width = width,
        });
    }

    /// Traverses all registered jump sites after all instruction are emitted
    /// and replaces placeholders with the actual code position of the target.
    void setJumpDests();

    size_t currentPosition() const { return binary.size(); }

    AssemblyStream const& stream;
    std::unordered_map<std::string, size_t>& sym;
    utl::vector<u8> binary;

    /// Maps Label ID to Code position
    utl::hashmap<LabelID, size_t> labels;
    /// List of all code position with a jump site
    utl::vector<Jumpsite> jumpsites;
    /// Address of the `start` or `main` function.
    size_t startAddress = 0;
};

} // namespace

AssemblerResult Asm::assemble(AssemblyStream const& astr) {
    AssemblerResult result;
    AsmContext ctx(astr, result.symbolTable);
    ctx.run();
    size_t dataSecSize = astr.dataSection().size();
    svm::ProgramHeader const header{
        .versionString = { svm::GlobalProgID },
        .size = sizeof(svm::ProgramHeader) + ctx.binary.size(),
        .startAddress = ctx.startAddress,
        .dataOffset = sizeof(svm::ProgramHeader),
        .textOffset = sizeof(svm::ProgramHeader) + dataSecSize,
        .FFIDeclOffset = sizeof(svm::ProgramHeader) + ctx.binary.size(),
    };
    result.program.resize(header.size);
    std::memcpy(result.program.data(), &header, sizeof(header));
    std::memcpy(result.program.data() + header.dataOffset,
                ctx.binary.data(),
                ctx.binary.size());
    return result;
}

void AsmContext::run() {
    /// We write the static data in the front of the binary
    binary = stream.dataSection();
    for (auto& block: stream) {
        if (block.isExternallyVisible()) {
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
    setJumpDests();
}

void AsmContext::dispatch(Instruction const& inst) {
    std::visit([this](auto& inst) { translate(inst); }, inst);
}

void AsmContext::translate(MoveInst const& mov) {
    auto const [opcode, size] = mapMove(mov.dest().valueType(),
                                        mov.source().valueType(),
                                        mov.numBytes());
    put(opcode);
    dispatch(mov.dest());
    dispatch(promote(mov.source(), size));
}

void AsmContext::translate(CMoveInst const& cmov) {
    auto const [opcode, size] = mapCMove(cmov.condition(),
                                         cmov.dest().valueType(),
                                         cmov.source().valueType(),
                                         cmov.numBytes());
    put(opcode);
    dispatch(cmov.dest());
    dispatch(promote(cmov.source(), size));
}

void AsmContext::translate(JumpInst const& jmp) {
    OpCode const opcode = mapJump(jmp.condition());
    put(opcode);
    put(LabelPlaceholder{}, jmp.target(), StaticAddressWidth);
    return;
}

void AsmContext::translate(CallInst const& call) {
    OpCode const opcode = mapCall(call.dest().valueType());
    put(opcode);
    dispatch(call.dest());
    put<u8>(call.regPtrOffset());
}

void AsmContext::translate(CallExtInst const& call) {
    put(OpCode::callExt);
    put<u8>(call.regPtrOffset());
    put<u8>(call.slot());
    put<u16>(call.index());
}

void AsmContext::translate(ReturnInst const& ret) { put(OpCode::ret); }

void AsmContext::translate(TerminateInst const& term) {
    put(OpCode::terminate);
}

void AsmContext::translate(LIncSPInst const& lincsp) {
    put(OpCode::lincsp);
    dispatch(lincsp.dest());
    dispatch(lincsp.offset());
}

void AsmContext::translate(LEAInst const& lea) {
    put(OpCode::lea);
    dispatch(lea.dest());
    dispatch(lea.address());
}

void AsmContext::translate(CompareInst const& cmp) {
    OpCode const opcode = mapCompare(cmp.type(),
                                     promote(cmp.lhs().valueType(), 8),
                                     promote(cmp.rhs().valueType(), 8),
                                     cmp.width());
    put(opcode);
    dispatch(promote(cmp.lhs(), 8));
    dispatch(promote(cmp.rhs(), 8));
}

void AsmContext::translate(TestInst const& test) {
    OpCode const opcode = mapTest(test.type(), test.width());
    put(opcode);
    SC_ASSERT(test.operand().is<RegisterIndex>(),
              "Can only test values in registers");
    dispatch(std::get<RegisterIndex>(test.operand()));
}

void AsmContext::translate(SetInst const& set) {
    OpCode const opcode = mapSet(set.operation());
    put(opcode);
    dispatch(set.dest());
}

void AsmContext::translate(UnaryArithmeticInst const& inst) {
    switch (inst.operation()) {
    case UnaryArithmeticOperation::LogicalNot:
        put(OpCode::lnt);
        break;
    case UnaryArithmeticOperation::BitwiseNot:
        put(OpCode::bnt);
        break;
    case UnaryArithmeticOperation::Negate:
        switch (inst.width()) {
        case 1:
            put(OpCode::neg8);
            break;
        case 2:
            put(OpCode::neg16);
            break;
        case 4:
            put(OpCode::neg32);
            break;
        case 8:
            put(OpCode::neg64);
            break;
        default:
            SC_UNREACHABLE();
        }
        break;
    case UnaryArithmeticOperation::_count:
        SC_UNREACHABLE();
    }
    translate(inst.operand());
}

void AsmContext::translate(ArithmeticInst const& inst) {
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

void AsmContext::translate(TruncExtInst const& inst) {
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
            SC_EXPECT(inst.type() == Type::Float);
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

void AsmContext::translate(ConvertInst const& inst) {
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

void AsmContext::dispatch(Value const& value) {
    std::visit([this](auto& value) { translate(value); }, value);
}

void AsmContext::translate(RegisterIndex const& regIdx) {
    put<u8>(regIdx.value());
}

void AsmContext::translate(MemoryAddress const& memAddr) {
    put<u8>(memAddr.baseptrRegisterIndex());
    put<u8>(memAddr.offsetCountRegisterIndex());
    put<u8>(memAddr.constantOffsetMultiplier());
    put<u8>(memAddr.constantInnerOffset());
}

void AsmContext::translate(Value8 const& value) { put<u8>(value.value()); }

void AsmContext::translate(Value16 const& value) { put<u16>(value.value()); }

void AsmContext::translate(Value32 const& value) { put<u32>(value.value()); }

void AsmContext::translate(Value64 const& value) { put<u64>(value.value()); }

void AsmContext::translate(LabelPosition const& pos) {
    auto width = [&] {
        using enum LabelPosition::Kind;
        switch (pos.kind()) {
        case Static:
            return StaticAddressWidth;
        case Dynamic:
            return DynamicAddressWidth;
        }
    }();
    put(LabelPlaceholder{}, pos.ID(), width);
}

template <typename T>
static void store(void* dest, T const& t) {
    std::memcpy(dest, &t, sizeof(T));
}

void AsmContext::setJumpDests() {
    for (auto const& [position, targetID, width]: jumpsites) {
        auto const itr = labels.find(targetID);
        SC_ASSERT(itr != labels.end(), "Use of undeclared label");
        size_t const targetPosition = itr->second;
        std::memcpy(&binary[position], &targetPosition, width);
    }
}

std::string Asm::generateDebugSymbols(AssemblyStream const& stream) {
    auto* list = std::any_cast<dbi::SourceFileList>(&stream.metadata());
    auto sourceLocations =
        stream | ranges::views::join |
        ranges::views::transform([](Instruction const& inst) {
            return std::any_cast<SourceLocation>(&inst.metadata());
        }) |
        ranges::to<std::vector>;
    return dbi::serialize(list, sourceLocations);
}
