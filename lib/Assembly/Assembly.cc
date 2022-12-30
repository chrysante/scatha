#include "Assembly/Assembly.h"

#include <bit>
#include <ostream>

#include <utl/utility.hpp>

#include "Assembly/AssemblerIssue.h"

namespace scatha::assembly {

std::ostream& operator<<(std::ostream& str, Instruction i) {
    // clang-format off
    return str << UTL_SERIALIZE_ENUM(i, {
        { Instruction::enterFn,         "enterFn" },
        { Instruction::setBrk,          "setBrk" },
        { Instruction::call,            "call" },
        { Instruction::ret,             "ret" },
        { Instruction::terminate,       "terminate" },
        { Instruction::mov,             "mov" },
        { Instruction::storeRegAddress, "storeRegAddress" },
        { Instruction::jmp,             "jmp" },
        { Instruction::je,              "je" },
        { Instruction::jne,             "jne" },
        { Instruction::jl,              "jl" },
        { Instruction::jle,             "jle" },
        { Instruction::jg,              "jg" },
        { Instruction::jge,             "jge" },
        { Instruction::ucmp,            "ucmp" },
        { Instruction::icmp,            "icmp" },
        { Instruction::fcmp,            "fcmp" },
        { Instruction::itest,           "itest" },
        { Instruction::utest,           "utest" },
        { Instruction::sete,            "sete" },
        { Instruction::setne,           "setne" },
        { Instruction::setl,            "setl" },
        { Instruction::setle,           "setle" },
        { Instruction::setg,            "setg" },
        { Instruction::setge,           "setge" },
        { Instruction::lnt,             "lnt" },
        { Instruction::bnt,             "bnt" },
        { Instruction::add,             "add" },
        { Instruction::sub,             "sub" },
        { Instruction::mul,             "mul" },
        { Instruction::div,             "div" },
        { Instruction::idiv,            "idiv" },
        { Instruction::rem,             "rem" },
        { Instruction::irem,            "irem" },
        { Instruction::fadd,            "fadd" },
        { Instruction::fsub,            "fsub" },
        { Instruction::fmul,            "fmul" },
        { Instruction::fdiv,            "fdiv" },
        { Instruction::sl,              "sl" },
        { Instruction::sr,              "sr" },
        { Instruction::And,             "and" },
        { Instruction::Or,              "or" },
        { Instruction::XOr,             "xor" },
        { Instruction::callExt,         "callExt" },
    });
    // clang-format on
}

void validate(Marker m_, size_t line) {
    auto const m = utl::to_underlying(m_);
    if (m >= utl::to_underlying(Marker::EndOfProgram)) {
        throw InvalidMarker(m_, line);
    }
    if (!std::has_single_bit(m)) {
        throw InvalidMarker(m_, line);
    }
}

std::ostream& operator<<(std::ostream& str, Label l) {
    return str << "LABEL: " << l.index << std::endl;
}

std::ostream& operator<<(std::ostream& str, RegisterIndex r) {
    return str << "R[" << +r.index << "]";
}

std::ostream& operator<<(std::ostream& str, MemoryAddress address) {
    return str << "MEMORY[R[" << +address.ptrRegIdx << "] + " << +address.offset << " * (1 << " << +address.offsetShift
               << ")]";
}

std::ostream& operator<<(std::ostream& str, Value8 v) {
    return str << +v.value;
}

std::ostream& operator<<(std::ostream& str, Value16 v) {
    return str << v.value;
}

std::ostream& operator<<(std::ostream& str, Value32 v) {
    return str << v.value;
}

std::ostream& operator<<(std::ostream& str, Value64 v) {
    switch (v.type) {
    case Value64::UnsignedIntegral: return str << v.value;
    case Value64::SignedIntegral: return str << static_cast<i64>(v.value);
    case Value64::FloatingPoint: return str << utl::bit_cast<f64>(v.value);
    }
}

std::ostream& operator<<(std::ostream& str, Marker m) {
    // clang-format off
    switch (m) {
    case Marker::Instruction:   return str << "Instruction";
    case Marker::Label:         return str << "Label";
    case Marker::RegisterIndex: return str << "RegisterIndex";
    case Marker::MemoryAddress: return str << "MemoryAddress";
    case Marker::Value8:        return str << "Value8";
    case Marker::Value16:       return str << "Value16";
    case Marker::Value32:       return str << "Value32";
    case Marker::Value64:       return str << "Value64";
    case Marker::EndOfProgram:  return str << "EndOfProgram";
    case Marker::none: SC_UNREACHABLE();
    }
    // clang-format on
}

std::ostream& operator<<(std::ostream& str, Element const& e) {
    return std::visit([&](auto const& x) -> decltype(auto) { return str << x; }, e);
}

} // namespace scatha::assembly
