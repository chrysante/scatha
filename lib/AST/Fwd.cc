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
    switch (op) {
    case Assignment:
        [[fallthrough]];
    case AddAssignment:
        [[fallthrough]];
    case SubAssignment:
        [[fallthrough]];
    case MulAssignment:
        [[fallthrough]];
    case DivAssignment:
        [[fallthrough]];
    case RemAssignment:
        [[fallthrough]];
    case LSAssignment:
        [[fallthrough]];
    case RSAssignment:
        [[fallthrough]];
    case AndAssignment:
        [[fallthrough]];
    case OrAssignment:
        [[fallthrough]];
    case XOrAssignment:
        return true;
    default:
        return false;
    }
}

bool ast::isArithmeticAssignment(BinaryOperator op) {
    return op != BinaryOperator::Assignment && isAssignment(op);
}

BinaryOperator ast::toNonAssignment(BinaryOperator op) {
    using enum BinaryOperator;
    switch (op) {
    case AddAssignment:
        return Addition;
    case SubAssignment:
        return Subtraction;
    case MulAssignment:
        return Multiplication;
    case DivAssignment:
        return Division;
    case RemAssignment:
        return Remainder;
    case LSAssignment:
        return LeftShift;
    case RSAssignment:
        return RightShift;
    case AndAssignment:
        return BitwiseAnd;
    case OrAssignment:
        return BitwiseOr;
    case XOrAssignment:
        return BitwiseXOr;
    default:
        SC_UNREACHABLE();
    }
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
