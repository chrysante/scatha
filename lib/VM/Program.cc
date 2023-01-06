#include "VM/Program.h"

#include <iomanip>
#include <iostream>
#include <ostream>
#include <span>

#include <utl/strcat.hpp>

#include "Basic/Memory.h"
#include "VM/OpCode.h"

namespace scatha::vm {

void print(Program const& p) {
    print(p, std::cout);
}

template <typename T>
static constexpr std::string_view typeToStr() {
#define SC_TYPETOSTR_CASE(type) \
else if constexpr (std::is_same_v<T, type>) { return #type; }
    if constexpr (false) {}
    SC_TYPETOSTR_CASE(u8)
    SC_TYPETOSTR_CASE(u16)
    SC_TYPETOSTR_CASE(u32)
    SC_TYPETOSTR_CASE(u64)
    SC_TYPETOSTR_CASE(i8)
    SC_TYPETOSTR_CASE(i16)
    SC_TYPETOSTR_CASE(i32)
    SC_TYPETOSTR_CASE(i64)
    else { static_assert(!std::is_same_v<T, T>); }
#undef SC_TYPETOSTR_CASE
}

template <typename T>
static auto readAs(std::span<u8 const> data, size_t offset) {
    return +read<T>(&data[offset]);
}

template <typename T>
static auto printAs(std::span<u8 const> data, size_t offset) {
    return utl::strcat("(", typeToStr<T>(), ")",  readAs<T>(data, offset));
}

void print(Program const& p, std::ostream& str) {
    std::span<u8 const> data = p.instructions;

    auto printMemoryAcccess = [&](size_t i) {
        str << "memory[R[" << printAs<u8>(data, i) << "] + " << readAs<u8>(data, i + 1) << " * "
            << (1 << readAs<u8>(data, i + 2)) << "]";
    };

    for (size_t i = 0; i < data.size();) {
        OpCode const opcode = static_cast<OpCode>(data[i]);
        str << std::setw(3) << i << ": " << opcode << " ";

        auto const opcodeClass = classify(opcode);
        switch (opcodeClass) {
            using enum OpCodeClass;
        case RR: str << "R[" << printAs<u8>(data, i + 1) << "], R[" << printAs<u8>(data, i + 2) << "]"; break;
        case RV: str << "R[" << printAs<u8>(data, i + 1) << "], " << printAs<u64>(data, i + 2); break;
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
            case OpCode::alloca_: str << printAs<u8>(data, i + 1) << ", " << printAs<u8>(data, i + 2); break;
            case OpCode::call: str << printAs<i32>(data, i + 1) << ", " << printAs<u8>(data, i + 5); break;
            case OpCode::ret: break;
            case OpCode::terminate: break;
            case OpCode::callExt:
                str << printAs<u8>(data, i + 1) << ", " << printAs<u8>(data, i + 2) << ", "
                    << printAs<u16>(data, i + 3);
                break;
            default: SC_UNREACHABLE();
            }
            break;
        case _count: SC_DEBUGFAIL();
        }

        str << '\n';
        i += codeSize(opcode);
    }
}

} // namespace scatha::vm
