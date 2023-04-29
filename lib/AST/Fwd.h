#ifndef SCATHA_AST_FWD_H_
#define SCATHA_AST_FWD_H_

#include <iosfwd>
#include <string_view>

#include <scatha/Common/Base.h>
#include <scatha/Common/Dyncast.h>

namespace scatha::ast {

///
/// **Forward Declaration of all AST nodes**
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

/// List of all unary operators in prefix notation
enum class UnaryPrefixOperator {
#define SC_UNARY_OPERATOR_DEF(name, _) name,
#include <scatha/AST/Lists.def>
    _count
};

SCATHA_API std::string_view toString(UnaryPrefixOperator);

SCATHA_API std::ostream& operator<<(std::ostream&, UnaryPrefixOperator);

/// List of all binary operators in infix notation
enum class BinaryOperator {
#define SC_BINARY_OPERATOR_DEF(name, _) name,
#include <scatha/AST/Lists.def>
    _count
};

SCATHA_API std::string_view toString(BinaryOperator);

SCATHA_API std::ostream& operator<<(std::ostream&, BinaryOperator);

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

} // namespace scatha::ast

namespace scatha::internal {

/// Insulated call to `delete` on the most derived base of \p *astNode
SCATHA_API void privateDelete(ast::AbstractSyntaxTree* astNode);

} // namespace scatha::internal

#endif // SCATHA_AST_FWD_H_
