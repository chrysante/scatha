#include "IC/ThreeAddressStatement.h"

#include <ostream>
#include <string_view>

#include <utl/utility.hpp>

namespace scatha::ic {

FunctionLabel::FunctionLabel(ast::FunctionDefinition const &def): _functionID(def.symbolID) {
    _parameters.reserve(def.parameters.size());
    for (auto &p : def.parameters) {
        _parameters.push_back({p->symbolID, p->typeID});
    }
}

std::string_view toString(Operation op) {
    return UTL_SERIALIZE_ENUM(op, {{Operation::mov, "mov"},
                                   {Operation::param, "param"},
                                   {Operation::getResult, "getResult"},
                                   {Operation::call, "call"},
                                   {Operation::ret, "ret"},
                                   {Operation::add, "add"},
                                   {Operation::sub, "sub"},
                                   {Operation::mul, "mul"},
                                   {Operation::div, "div"},
                                   {Operation::idiv, "idiv"},
                                   {Operation::rem, "rem"},
                                   {Operation::irem, "irem"},
                                   {Operation::fadd, "fadd"},
                                   {Operation::fsub, "fsub"},
                                   {Operation::fmul, "fmul"},
                                   {Operation::fdiv, "fdiv"},
                                   {Operation::sl, "sl"},
                                   {Operation::sr, "sr"},
                                   {Operation::And, "and"},
                                   {Operation::Or, "or"},
                                   {Operation::XOr, "xor"},
                                   {Operation::eq, "eq"},
                                   {Operation::neq, "neq"},
                                   {Operation::ils, "ils"},
                                   {Operation::ileq, "ileq"},
                                   {Operation::ig, "ig"},
                                   {Operation::igeq, "igeq"},
                                   {Operation::uls, "uls"},
                                   {Operation::uleq, "uleq"},
                                   {Operation::ug, "ug"},
                                   {Operation::ugeq, "ugeq"},
                                   {Operation::feq, "feq"},
                                   {Operation::fneq, "fneq"},
                                   {Operation::fls, "fls"},
                                   {Operation::fleq, "fleq"},
                                   {Operation::fg, "fg"},
                                   {Operation::fgeq, "fgeq"},
                                   {Operation::lnt, "lnt"},
                                   {Operation::bnt, "bnt"},
                                   {Operation::jmp, "jmp"},
                                   {Operation::ifPlaceholder, "ifPlaceholder(this should not be printed)"}});
}

std::ostream &operator<<(std::ostream &str, Operation op) { return str << toString(op); }

int           argumentCount(Operation op) {
              return UTL_MAP_ENUM(
                  op, int, {{Operation::mov, 1},          {Operation::param, 1}, {Operation::getResult, 0}, {Operation::call, 1},
                            {Operation::ret, 1},          {Operation::add, 2},   {Operation::sub, 2},       {Operation::mul, 2},
                            {Operation::div, 2},          {Operation::idiv, 2},  {Operation::rem, 2},       {Operation::irem, 2},
                            {Operation::fadd, 2},         {Operation::fsub, 2},  {Operation::fmul, 2},      {Operation::fdiv, 2},
                            {Operation::sl, 2},           {Operation::sr, 2},    {Operation::And, 2},       {Operation::Or, 2},
                            {Operation::XOr, 2},          {Operation::eq, 2},    {Operation::neq, 2},       {Operation::ils, 2},
                            {Operation::ileq, 2},         {Operation::ig, 2},    {Operation::igeq, 2},      {Operation::uls, 2},
                            {Operation::uleq, 2},         {Operation::ug, 2},    {Operation::ugeq, 2},      {Operation::feq, 2},
                            {Operation::fneq, 2},         {Operation::fls, 2},   {Operation::fleq, 2},      {Operation::fg, 2},
                            {Operation::fgeq, 2},         {Operation::lnt, 1},   {Operation::bnt, 1},       {Operation::jmp, 1},
                            {Operation::ifPlaceholder, 1}});
}

bool isJump(Operation op) {
    using enum Operation;
    return op == call || op == jmp;
}

bool isRelop(Operation op) {
    using enum Operation;
    return (utl::to_underlying(op) >= utl::to_underlying(eq) && utl::to_underlying(op) <= utl::to_underlying(fgeq));
}

Operation reverseRelop(Operation op) {
    using enum Operation;
    switch (op) {
    case eq:
        return neq;
    case neq:
        return eq;
    case ils:
        return igeq;
    case ileq:
        return ig;
    case uls:
        return ugeq;
    case uleq:
        return ug;
    case feq:
        return fneq;
    case fneq:
        return feq;
    case fls:
        return fgeq;
    case fleq:
        return fg;
        SC_NO_DEFAULT_CASE();
    }
}

} // namespace scatha::ic
