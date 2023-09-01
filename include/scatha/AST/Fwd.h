#ifndef SCATHA_AST_FWD_H_
#define SCATHA_AST_FWD_H_

#include <iosfwd>
#include <string_view>

#include <scatha/Common/Base.h>
#include <scatha/Common/Dyncast.h>

namespace scatha::ast {

///
/// # Forward Declaration of all AST nodes
///

#define SC_ASTNODE_DEF(node, _) class node;
#include <scatha/AST/Lists.def>

/// List of all  AST node types.
enum class NodeType {
#define SC_ASTNODE_DEF(node, _) node,
#include <scatha/AST/Lists.def>
    _count
};

SCATHA_API std::string_view toString(NodeType);

SCATHA_API std::ostream& operator<<(std::ostream&, NodeType);

} // namespace scatha::ast

/// Map types to enum values.
#define SC_ASTNODE_DEF(type, Abstractness)                                     \
    SC_DYNCAST_MAP(::scatha::ast::type,                                        \
                   ::scatha::ast::NodeType::type,                              \
                   Abstractness)
#include <scatha/AST/Lists.def>

namespace scatha::ast {

/// List of all kinds of literals
enum class LiteralKind {
#define SC_LITERAL_KIND_DEF(kind, _) kind,
#include <scatha/AST/Lists.def>
};

SCATHA_API std::string_view toString(LiteralKind);

SCATHA_API std::ostream& operator<<(std::ostream&, LiteralKind);

/// List of all unary operators
enum class UnaryOperator {
#define SC_UNARY_OPERATOR_DEF(name, _) name,
#include <scatha/AST/Lists.def>
    _count
};

SCATHA_API std::string_view toString(UnaryOperator);

SCATHA_API std::ostream& operator<<(std::ostream&, UnaryOperator);

/// List of unary operator notation
enum class UnaryOperatorNotation {
#define SC_UNARY_OPERATOR_NOTATION_DEF(name, _) name,
#include <scatha/AST/Lists.def>
    _count
};

SCATHA_API std::string_view toString(UnaryOperatorNotation);

SCATHA_API std::ostream& operator<<(std::ostream&, UnaryOperatorNotation);

/// List of all binary operators in infix notation
enum class BinaryOperator {
#define SC_BINARY_OPERATOR_DEF(name, _) name,
#include <scatha/AST/Lists.def>
    _count
};

SCATHA_API std::string_view toString(BinaryOperator);

SCATHA_API std::ostream& operator<<(std::ostream&, BinaryOperator);

/// \Returns `true` if \p op is an assignment or arithmetic assignment
/// operation
bool isAssignment(BinaryOperator op);

/// \Returns `true` if \p op is an arithmetic assignment operation
bool isArithmeticAssignment(BinaryOperator op);

/// \Returns The non-assignment version of \p op
/// \Pre \p op must be an arithmetic assignment operator
BinaryOperator toNonAssignment(BinaryOperator op);

/// List of all access specifiers
enum class AccessSpec {
#define SC_ACCESS_SPEC_DEF(name, _) name,
#include <scatha/AST/Lists.def>
    _count
};

SCATHA_API std::string_view toString(AccessSpec);

SCATHA_API std::ostream& operator<<(std::ostream&, AccessSpec);

enum class LoopKind { For, While, DoWhile };

std::string_view toString(LoopKind loopKind);

std::ostream& operator<<(std::ostream& ostream, LoopKind loopKind);

/// Insulated call to `delete` on the most derived base of \p astNode
SCATHA_API void privateDelete(ast::AbstractSyntaxTree* astNode);

} // namespace scatha::ast

#endif // SCATHA_AST_FWD_H_
