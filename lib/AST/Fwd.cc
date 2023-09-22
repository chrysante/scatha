#include "AST/Fwd.h"

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

std::string_view ast::toString(LiteralKind kind) {
    return std::array{
#define SC_LITERAL_KIND_DEF(kind, str) std::string_view(str),
#include "AST/Lists.def"
    }[static_cast<size_t>(kind)];
}

std::ostream& ast::operator<<(std::ostream& str, LiteralKind kind) {
    return str << toString(kind);
}

std::string_view ast::toString(UnaryOperator op) {
    return std::array{
#define SC_UNARY_OPERATOR_DEF(name, opStr) std::string_view(opStr),
#include "AST/Lists.def"
    }[static_cast<size_t>(op)];
}

std::ostream& ast::operator<<(std::ostream& str, UnaryOperator op) {
    return str << toString(op);
}

std::string_view ast::toString(UnaryOperatorNotation notation) {
    return std::array{
#define SC_UNARY_OPERATOR_NOTATION_DEF(name, str) std::string_view(str),
#include "AST/Lists.def"
    }[static_cast<size_t>(notation)];
}

std::ostream& ast::operator<<(std::ostream& str,
                              UnaryOperatorNotation notation) {
    return str << toString(notation);
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

bool ast::isAssignment(BinaryOperator op) {
    using enum BinaryOperator;
    return utl::to_underlying(op) >= utl::to_underlying(Assignment) &&
           utl::to_underlying(op) <= utl::to_underlying(XOrAssignment);
}

bool ast::isArithmeticAssignment(BinaryOperator op) {
    return op != BinaryOperator::Assignment && isAssignment(op);
}

BinaryOperator ast::toNonAssignment(BinaryOperator op) {
    using enum BinaryOperator;
    auto diff =
        utl::to_underlying(AddAssignment) - utl::to_underlying(Assignment);
    return BinaryOperator{ utl::to_underlying(op) - diff };
}

std::string_view ast::toString(LoopKind loopKind) {
    switch (loopKind) {
    case LoopKind::For:
        return "For";
    case LoopKind::While:
        return "While";
    case LoopKind::DoWhile:
        return "Do/While";
    }
}

std::ostream& ast::operator<<(std::ostream& ostream, LoopKind loopKind) {
    return ostream << toString(loopKind);
}
