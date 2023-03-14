#include "ProgramInternal.h"

#include <svm/Program.h>

#include <iomanip>
#include <iostream>
#include <ostream>
#include <span>

#include <utl/strcat.hpp>

#include "Memory.h"
#include <svm/OpCode.h>

using namespace svm;

Program::Program(u8 const* prog) {
    ProgramHeader header;
    std::memcpy(&header, prog, sizeof(header));
    size_t const dataSize = header.textOffset - header.dataOffset;
    data.resize(dataSize, utl::no_init);
    std::memcpy(data.data(), prog + header.dataOffset, dataSize);
    size_t const textSize = header.size - header.textOffset;
    instructions.resize(textSize, utl::no_init);
    std::memcpy(instructions.data(), prog + header.textOffset, textSize);
    start = header.start;
}

void svm::print(u8 const* program) {
    svm::print(program, std::cout);
}

template <typename T>
static constexpr std::string_view typeToStr() {
#define SVM_TYPETOSTR_CASE(type)                                               \
    else if constexpr (std::is_same_v<T, type>) {                              \
        return #type;                                                          \
    }
    if constexpr (false)
        ;
    SVM_TYPETOSTR_CASE(u8)
    SVM_TYPETOSTR_CASE(u16)
    SVM_TYPETOSTR_CASE(u32)
    SVM_TYPETOSTR_CASE(u64)
    SVM_TYPETOSTR_CASE(i8)
    SVM_TYPETOSTR_CASE(i16)
    SVM_TYPETOSTR_CASE(i32)
    SVM_TYPETOSTR_CASE(i64)
    else {
        static_assert(!std::is_same_v<T, T>);
    }
#undef SVM_TYPETOSTR_CASE
}

template <typename T>
static T readAs(std::span<u8 const> data, size_t offset) {
    return read<T>(&data[offset]);
}

template <typename T>
static auto printAs(auto data) {
    return utl::strcat("(", typeToStr<T>(), ")", +data);
}

template <typename T>
static auto printAs(std::span<u8 const> data, size_t offset) {
    return printAs<T>(readAs<T>(data, offset));
}

void svm::print(u8 const* progData, std::ostream& str) {
    Program const p(progData);
    std::span<u8 const> data = p.instructions;
    auto printMemoryAcccess  = [&](size_t i) {
        size_t const baseptrRegisterIndex     = readAs<u8>(data, i);
        size_t const offsetCountRegisterIndex = readAs<u8>(data, i + 1);
        u8 const constantOffsetMultiplier     = readAs<u8>(data, i + 2);
        u8 const constantInnerOffset          = readAs<u8>(data, i + 3);
        str << "*(ptr)R[" << printAs<u8>(baseptrRegisterIndex) << "]";
        if (offsetCountRegisterIndex != 0xFF) {
            str << " + (i64)R[" << printAs<u8>(offsetCountRegisterIndex)
                << "] * " << printAs<u8>(constantOffsetMultiplier);
        }
        str << " + " << printAs<u8>(constantInnerOffset);
    };
    for (size_t i = 0; i < data.size();) {
        OpCode const opcode = static_cast<OpCode>(data[i]);
        str << std::setw(3) << i << ": " << opcode << " ";

        auto const opcodeClass = classify(opcode);
        switch (opcodeClass) {
            using enum OpCodeClass;
        case RR:
            str << "R[" << printAs<u8>(data, i + 1) << "], R["
                << printAs<u8>(data, i + 2) << "]";
            break;
        case RV:
            str << "R[" << printAs<u8>(data, i + 1) << "], "
                << printAs<u64>(data, i + 2);
            break;
        case RM:
            str << "R[" << printAs<u8>(data, i + 1) << "], ";
            printMemoryAcccess(i + 2);
            break;
        case MR:
            printMemoryAcccess(i + 1);
            str << ", R[" << printAs<u8>(data, i + 4) << "]";
            break;
        case R: str << "R[" << printAs<u8>(data, i + 1) << "]"; break;
        case Jump: str << printAs<i32>(data, i + 1); break;
        case Other:
            switch (opcode) {
            case OpCode::alloca_:
                str << printAs<u8>(data, i + 1) << ", "
                    << printAs<u8>(data, i + 2);
                break;
            case OpCode::call:
                str << printAs<i32>(data, i + 1) << ", "
                    << printAs<u8>(data, i + 5);
                break;
            case OpCode::ret: break;
            case OpCode::terminate: break;
            case OpCode::callExt:
                str << printAs<u8>(data, i + 1) << ", "
                    << printAs<u8>(data, i + 2) << ", "
                    << printAs<u16>(data, i + 3);
                break;
            default: assert(false);
            }
            break;
        case _count: assert(false);
        }
        str << '\n';
        i += codeSize(opcode);
    }
}
