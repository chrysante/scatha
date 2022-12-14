// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_AST_COMMON_H_
#define SCATHA_AST_COMMON_H_

#include <iosfwd>
#include <string_view>

#include <scatha/Basic/Basic.h>
#include <scatha/Common/Dyncast.h>

namespace scatha::ast {

///
/// **Forward Declaration of all AST nodes**
///

#define SC_ASTNODE_DEF(node) class node;
#include <scatha/AST/Lists.def>

/// List of all  AST node types.
enum class NodeType {
#define SC_ASTNODE_DEF(node) node,
#include <scatha/AST/Lists.def>
    _count
};

SCATHA(API) std::string_view toString(NodeType);

SCATHA(API) std::ostream& operator<<(std::ostream&, NodeType);

} // namespace scatha::ast

#define SC_ASTNODE_DEF(type) SC_DYNCAST_MAP(::scatha::ast::type, ::scatha::ast::NodeType::type);
#include <scatha/AST/Lists.def>

namespace scatha::ast {

/// List of all unary operators in prefix notation
enum class UnaryPrefixOperator {
#define SC_UNARY_OPERATOR_DEF(name, _) name,
#include <scatha/AST/Lists.def>
    _count
};

SCATHA(API) std::string_view toString(UnaryPrefixOperator);

SCATHA(API) std::ostream& operator<<(std::ostream&, UnaryPrefixOperator);

/// List of all binary operators in infix notation
enum class BinaryOperator {
#define SC_BINARY_OPERATOR_DEF(name, _) name,
#include <scatha/AST/Lists.def>
    _count
};

SCATHA(API) std::string_view toString(BinaryOperator);

SCATHA(API) std::ostream& operator<<(std::ostream&, BinaryOperator);

/// Categories of entities in the AST module
enum class EntityCategory { Value, Type, _count };

SCATHA(API) std::ostream& operator<<(std::ostream&, EntityCategory);

} // namespace scatha::ast

#endif // SCATHA_AST_COMMON_H_
