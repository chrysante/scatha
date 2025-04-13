#include "scbinutil/ProgramView.h"

#include <iomanip>
#include <iostream>
#include <ostream>
#include <span>
#include <stdexcept>

#include <utl/hashtable.hpp>
#include <utl/strcat.hpp>
#include <utl/streammanip.hpp>

#include "scbinutil/OpCode.h"

using namespace scbinutil;

template <typename T>
static T load(void const* ptr) {
    std::aligned_storage_t<sizeof(T), alignof(T)> storage;
    std::memcpy(&storage, ptr, sizeof(T));
    return reinterpret_cast<T const&>(storage);
}

template <typename T>
static void store(void* dest, T const& t) {
    std::memcpy(dest, &t, sizeof(T));
}

[[noreturn]] static void unreachable() {
#if defined(__GNUC__)
    __builtin_unreachable();
#elif defined(_MSC_VER)
    __assume(false);
#else
    assert(false);
#endif
}

FFITrivialType FFIType::_sVoid = FFITrivialType(Kind::Void);
FFITrivialType FFIType::_sInt8 = FFITrivialType(Kind::Int8);
FFITrivialType FFIType::_sInt16 = FFITrivialType(Kind::Int16);
FFITrivialType FFIType::_sInt32 = FFITrivialType(Kind::Int32);
FFITrivialType FFIType::_sInt64 = FFITrivialType(Kind::Int64);
FFITrivialType FFIType::_sFloat = FFITrivialType(Kind::Float);
FFITrivialType FFIType::_sDouble = FFITrivialType(Kind::Double);
FFITrivialType FFIType::_sPointer = FFITrivialType(Kind::Pointer);

FFIType const* FFIType::Trivial(Kind kind) {
    switch (kind) {
    case Kind::Void:
        return Void();
    case Kind::Int8:
        return Int8();
    case Kind::Int16:
        return Int16();
    case Kind::Int32:
        return Int32();
    case Kind::Int64:
        return Int64();
    case Kind::Float:
        return Float();
    case Kind::Double:
        return Double();
    case Kind::Pointer:
        return Pointer();
    default:
        unreachable();
    }
}

namespace {

struct StructKey {
    std::vector<FFIType const*> elems;

    bool operator==(StructKey const&) const = default;
};

} // namespace

template <>
struct std::hash<StructKey> {
    size_t operator()(StructKey const& key) const {
        return utl::hash_combine_range(key.elems.begin(), key.elems.end());
    }
};

FFIType const* FFIType::Struct(std::span<FFIType const* const> types) {
    static utl::hashmap<StructKey, std::unique_ptr<FFIType>> map;
    StructKey key;
    key.elems.assign(types.begin(), types.end());
    auto& p = map[key];
    if (!p) {
        p = std::make_unique<FFIStructType>(std::move(key.elems));
    }
    return p.get();
}

template <typename T>
static T readAs(std::span<uint8_t const> data, size_t offset) {
    return load<T>(&data[offset]);
}

namespace {

struct LibDeclParser {
    std::span<uint8_t const> data;

    template <typename T>
    T read() {
        size_t size = sizeof(T);
        if (data.size() < size) {
            throw std::runtime_error("Unexpected binary end");
        }
        T result = readAs<T>(data, 0);
        data = data.subspan(size);
        return result;
    }

    std::vector<FFILibDecl> parse() {
        size_t numLibs = read<uint32_t>();
        std::vector<FFILibDecl> libs;
        for (size_t i = 0; i < numLibs; ++i) {
            libs.push_back(parseLibDecl());
        }
        return libs;
    }

    std::string parseString() {
        std::string text;
        while (true) {
            auto c = read<char>();
            if (c == '\0') {
                break;
            }
            text.push_back(c);
        }
        return text;
    }

    FFILibDecl parseLibDecl() {
        std::string name = parseString();
        size_t numFunctions = read<uint32_t>();
        std::vector<FFIDecl> FFIs;
        for (size_t i = 0; i < numFunctions; ++i) {
            FFIs.push_back(parseFFIDecl());
        }
        return { std::move(name), std::move(FFIs) };
    }

    FFIDecl parseFFIDecl() {
        auto name = parseString();
        size_t numArgs = read<uint8_t>();
        std::vector<FFIType const*> argTypes;
        for (size_t i = 0; i < numArgs; ++i) {
            argTypes.push_back(parseType());
        }
        auto retType = parseType();
        auto index = read<uint32_t>();
        return FFIDecl{ .name = name,
                        .argumentTypes = argTypes,
                        .returnType = retType,
                        .index = index };
    }

    FFIType const* parseType() {
        auto kind = FFIType::Kind{ read<uint8_t>() };
        if (FFIType::isTrivial(kind)) {
            return FFIType::Trivial(kind);
        }
        size_t numElems = read<uint16_t>();
        std::vector<FFIType const*> elems(numElems);
        for (auto& elem: elems) {
            elem = parseType();
        }
        return FFIType::Struct(std::move(elems));
    }
};

} // namespace

static std::vector<FFILibDecl> parseLibDecls(std::span<uint8_t const> data) {
    return LibDeclParser{ data }.parse();
}

ProgramView::ProgramView(uint8_t const* prog) {
    ProgramHeader header{};
    std::memcpy(&header, prog, sizeof(header));
    if (header.versionString[0] != GlobalProgID) {
        throw std::runtime_error("Invalid version string");
    }
    size_t dataSize = header.textOffset - header.dataOffset;
    size_t textSize = header.FFIDeclOffset - header.textOffset;
    size_t FFIDeclSize = header.size - header.FFIDeclOffset;
    size_t binarySize = dataSize + textSize;
    this->header = header;
    this->startAddress = header.startAddress;
    this->binary = std::span(prog + header.dataOffset, binarySize);
    this->data = std::span(prog + header.dataOffset, dataSize);
    this->text = std::span(prog + header.textOffset, textSize);
    this->libDecls =
        parseLibDecls(std::span(prog + header.FFIDeclOffset, FFIDeclSize));
}

void scbinutil::print(uint8_t const* program) {
    scbinutil::print(program, std::cout);
}

template <typename T>
static constexpr std::string_view typeToStr() {
#define SVM_TYPETOSTR_CASE(type)                                               \
    else if constexpr (std::is_same_v<T, type>) { return #type; }
    if constexpr (false) (void)0; /// First case for the macro to work.
    SVM_TYPETOSTR_CASE(uint8_t)
    SVM_TYPETOSTR_CASE(uint16_t)
    SVM_TYPETOSTR_CASE(uint32_t)
    SVM_TYPETOSTR_CASE(uint64_t)
    SVM_TYPETOSTR_CASE(int8_t)
    SVM_TYPETOSTR_CASE(int16_t)
    SVM_TYPETOSTR_CASE(int32_t)
    SVM_TYPETOSTR_CASE(int64_t)
    else { static_assert(!std::is_same_v<T, T>); }
#undef SVM_TYPETOSTR_CASE
}

template <typename T>
static auto printAs(auto data) {
    return utl::strcat("(", typeToStr<T>(), ")", +data);
}

template <typename T>
static auto printAs(std::span<uint8_t const> data, size_t offset) {
    return printAs<T>(readAs<T>(data, offset));
}

static constexpr utl::streammanip reg([](std::ostream& str,
                                         std::span<uint8_t const> text,
                                         size_t i) {
    str << "%" << +readAs<uint8_t>(text, i);
});

static constexpr utl::streammanip memoryAcccess(
    [](std::ostream& str, std::span<uint8_t const> text, size_t i) {
        size_t const offsetCountRegisterIndex = readAs<uint8_t>(text, i + 1);
        uint8_t const constantOffsetMultiplier = readAs<uint8_t>(text, i + 2);
        uint8_t const constantInnerOffset = readAs<uint8_t>(text, i + 3);
        str << "[ " << reg(text, i);
        if (offsetCountRegisterIndex != 0xFF) {
            str << " + " << reg(text, i + 1) << " * "
                << printAs<uint8_t>(constantOffsetMultiplier);
        }
        if (constantInnerOffset > 0) {
            str << " + " << printAs<uint8_t>(constantInnerOffset);
        }
        str << " ]";
    });

void scbinutil::print(uint8_t const* progData, std::ostream& str) {
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
            str << reg(text, i + 1) << ", " << printAs<uint64_t>(text, i + 2);
            break;
        case RV32:
            str << reg(text, i + 1) << ", " << printAs<uint32_t>(text, i + 2);
            break;
        case RV8:
            str << reg(text, i + 1) << ", " << printAs<uint8_t>(text, i + 2);
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
            str << printAs<int32_t>(text, i + 1);
            break;
        case Other:
            switch (opcode) {
            case OpCode::lincsp:
                str << reg(text, i + 1) << ", " << printAs<uint16_t>(i + 2);
                break;
            case OpCode::call:
                str << printAs<int32_t>(text, i + 1) << ", "
                    << printAs<uint8_t>(text, i + 5);
                break;
            case OpCode::icallr:
                str << reg(text, i + 1) << ", "
                    << printAs<uint8_t>(text, i + 2);
                break;
            case OpCode::icallm:
                str << memoryAcccess(text, i + 1) << ", "
                    << printAs<uint8_t>(text, i + 5);
                break;
            case OpCode::ret:
                break;
            case OpCode::terminate:
                break;
            case OpCode::cfng:
                [[fallthrough]];
            case OpCode::cbltn:
                str << printAs<uint8_t>(text, i + 1) << ", "
                    << printAs<uint16_t>(text, i + 2);
                break;
            default:
                unreachable();
            }
            break;
        }
        str << '\n';
        i += codeSize(opcode);
    }
}
