// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_AST_BASE_H_
#define SCATHA_AST_BASE_H_

#include <concepts>
#include <memory>
#include <string>

#include <scatha/AST/Common.h>
#include <scatha/Common/SourceLocation.h>
#include <scatha/Common/Token.h>
#include <scatha/Common/UniquePtr.h>

namespace scatha::ast {

namespace internal {

class Decoratable {
public:
    bool isDecorated() const { return decorated; }

protected:
    void expectDecorated() const { SC_EXPECT(isDecorated(), "Requested decoration on undecorated node."); }
    void markDecorated() { decorated = true; }

private:
    bool decorated = false;
};

} // namespace internal

/// **Base class for all nodes in the AST**
/// Every derived class must specify its runtime type in the constructor via the \p NodeType enum.
class SCATHA(API) AbstractSyntaxTree: public internal::Decoratable {
public:
    /// Runtime type of this node
    NodeType nodeType() const { return _type; }

    /// Token associated with this node
    Token const& token() const { return _token; }

    /// Source location object associated with this node. Same as
    /// token().sourceLocation
    SourceLocation sourceLocation() const { return token().sourceLocation; }

protected:
    explicit AbstractSyntaxTree(NodeType type, Token const& token): _type(type), _token(token) {}

    void setToken(Token token) { _token = std::move(token); }

private:
    NodeType _type;
    Token _token;
};

// For dyncast compatibilty
NodeType dyncast_get_type(std::derived_from<AbstractSyntaxTree> auto const& node) {
    return node.nodeType();
}

} // namespace scatha::ast

#endif // SCATHA_AST_BASE_H_
