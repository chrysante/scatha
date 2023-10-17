#include "svm/ProgramInternal.h"

#include <iomanip>
#include <iostream>
#include <ostream>
#include <span>
#include <stdexcept>

#include <utl/strcat.hpp>
#include <utl/streammanip.hpp>

#include "svm/Memory.h"
#include "svm/OpCodeInternal.h"

using namespace svm;

ProgramView::ProgramView(u8 const* prog) {
    ProgramHeader header;
    std::memcpy(&header, prog, sizeof(header));
    if (header.versionString[0] != GlobalProgID) {
        throw std::runtime_error("Invalid version string");
    }
    auto* binaryBegin = prog + header.dataOffset;
    size_t dataSize = header.textOffset - header.dataOffset;
    size_t textSize = header.size - header.textOffset;
    size_t binarySize = dataSize + textSize;
    this->binary = std::span(binaryBegin, binarySize);
    this->data = std::span(binaryBegin, dataSize);
    this->text = std::span(binaryBegin + dataSize, textSize);
    this->startAddress = header.startAddress;
}

void svm::print(u8 const* program) { svm::print(program, std::cout); }

template <typename T>
static constexpr std::string_view typeToStr() {
#define SVM_TYPETOSTR_CASE(type)                                               \
    else if constexpr (std::is_same_v<T, type>) { return #type; }
    if constexpr (false) (void)0; /// First case for the macro to work.
    SVM_TYPETOSTR_CASE(u8)
    SVM_TYPETOSTR_CASE(u16)
    SVM_TYPETOSTR_CASE(u32)
    SVM_TYPETOSTR_CASE(u64)
    SVM_TYPETOSTR_CASE(i8)
    SVM_TYPETOSTR_CASE(i16)
    SVM_TYPETOSTR_CASE(i32)
    SVM_TYPETOSTR_CASE(i64)
    else { static_assert(!std::is_same_v<T, T>); }
#undef SVM_TYPETOSTR_CASE
}

template <typename T>
static T readAs(std::span<u8 const> data, size_t offset) {
    return load<T>(&data[offset]);
}

template <typename T>
static auto printAs(auto data) {
    return utl::strcat("(", typeToStr<T>(), ")", +data);
}

template <typename T>
static auto printAs(std::span<u8 const> data, size_t offset) {
    return printAs<T>(readAs<T>(data, offset));
}

static constexpr utl::streammanip reg([](std::ostream& str,
                                         std::span<u8 const> text,
                                         size_t i) {
    str << "%" << +readAs<u8>(text, i);
});

static constexpr utl::streammanip memoryAcccess([](std::ostream& str,
                                                   std::span<u8 const> text,
                                                   size_t i) {
    size_t const offsetCountRegisterIndex = readAs<u8>(text, i + 1);
    u8 const constantOffsetMultiplier = readAs<u8>(text, i + 2);
    u8 const constantInnerOffset = readAs<u8>(text, i + 3);
    str << "[ " << reg(text, i);
    if (offsetCountRegisterIndex != 0xFF) {
        str << " + " << reg(text, i + 1) << " * "
            << printAs<u8>(constantOffsetMultiplier);
    }
    if (constantInnerOffset > 0) {
        str << " + " << printAs<u8>(constantInnerOffset);
    }
    str << " ]";
});

void svm::print(u8 const* progData, std::ostream& str) {
    ProgramView const p(progData);

    str << ".data:\n";
    for (unsigned b: p.data) {
        str << std::hex << b;
    }
    if (!p.data.empty()) {
        str << "\n";
    }
    str << "\n";

    str << ".text:\n";
    auto text = p.text;
    for (size_t i = 0; i < text.size();) {
        OpCode const opcode = static_cast<OpCode>(text[i]);
        str << std::setw(3) << i << ": " << opcode << " ";

        auto const opcodeClass = classify(opcode);
        switch (opcodeClass) {
            using enum OpCodeClass;
        case RR:
            str << reg(text, i + 1) << ", " << reg(text, i + 2);
            break;
        case RV64:
            str << reg(text, i + 1) << ", " << printAs<u64>(text, i + 2);
            break;
        case RV32:
            str << reg(text, i + 1) << ", " << printAs<u32>(text, i + 2);
            break;
        case RV8:
            str << reg(text, i + 1) << ", " << printAs<u8>(text, i + 2);
            break;
        case RM:
            str << reg(text, i + 1) << ", " << memoryAcccess(text, i + 2);
            break;
        case MR:
            str << memoryAcccess(text, i + 1) << ", " << reg(text, i + 4);
            break;
        case R:
            str << reg(text, i + 1);
            break;
        case Jump:
            str << printAs<i32>(text, i + 1);
            break;
        case Other:
            switch (opcode) {
            case OpCode::lincsp:
                str << reg(text, i + 1) << ", " << printAs<u16>(i + 2);
                break;
            case OpCode::call:
                str << printAs<i32>(text, i + 1) << ", "
                    << printAs<u8>(text, i + 5);
                break;
            case OpCode::icallr:
                str << reg(text, i + 1) << ", " << printAs<u8>(text, i + 2);
                break;
            case OpCode::icallm:
                str << memoryAcccess(text, i + 1) << ", "
                    << printAs<u8>(text, i + 5);
                break;
            case OpCode::ret:
                break;
            case OpCode::terminate:
                break;
            case OpCode::callExt:
                str << printAs<u8>(text, i + 1) << ", "
                    << printAs<u8>(text, i + 2) << ", "
                    << printAs<u16>(text, i + 3);
                break;
            default:
                assert(false);
            }
            break;
        case _count:
            assert(false);
        }
        str << '\n';
        i += codeSize(opcode);
    }
}
