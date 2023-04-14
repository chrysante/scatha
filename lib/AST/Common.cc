#include "AST/Common.h"

#include <array>
#include <ostream>

#include <utl/utility.hpp>

using namespace scatha;
using namespace ast;

std::string_view ast::toString(NodeType t) {
    return std::array{
#define SC_ASTNODE_DEF(node, _) std::string_view(#node),
#include "AST/Lists.def"
    }[static_cast<size_t>(t)];
}

std::ostream& ast::operator<<(std::ostream& str, NodeType t) {
    return str << toString(t);
}

std::string_view ast::toString(UnaryPrefixOperator op) {
    return std::array{
#define SC_UNARY_OPERATOR_DEF(name, opStr) std::string_view(opStr),
#include "AST/Lists.def"
    }[static_cast<size_t>(op)];
}

std::ostream& ast::operator<<(std::ostream& str, UnaryPrefixOperator op) {
    return str << toString(op);
}

std::string_view ast::toString(BinaryOperator op) {
    return std::array{
#define SC_BINARY_OPERATOR_DEF(name, opStr) std::string_view(opStr),
#include "AST/Lists.def"
    }[static_cast<size_t>(op)];
}

std::ostream& ast::operator<<(std::ostream& str, BinaryOperator op) {
    return str << toString(op);
}

std::ostream& ast::operator<<(std::ostream& str, EntityCategory cat) {
    // clang-format off
    return str << UTL_SERIALIZE_ENUM(cat, {
        { EntityCategory::Value, "Value" },
        { EntityCategory::Type,  "Type" },
    });
    // clang-format on
}

std::ostream& ast::operator<<(std::ostream& str, ValueCategory cat) {
    // clang-format off
    return str << UTL_SERIALIZE_ENUM(cat, {
        { ValueCategory::None,   "None" },
        { ValueCategory::LValue, "LValue" },
        { ValueCategory::RValue, "RValue" },
    });
    // clang-format on
}

std::string_view ast::toString(AccessSpec spec) {
    return std::array{
#define SC_ACCESS_SPEC_DEF(spec, str) std::string_view(str),
#include "AST/Lists.def"
    }[static_cast<size_t>(spec)];
}

std::ostream& ast::operator<<(std::ostream& str, AccessSpec spec) {
    return str << toString(spec);
}
