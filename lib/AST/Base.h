#pragma once

#ifndef SCATHA_AST_BASE_H_
#define SCATHA_AST_BASE_H_

#include <memory>
#include <string>

#include "AST/Common.h"
#include "Common/SourceLocation.h"
#include "Common/Token.h"

namespace scatha::ast {

// Forward declaration for definition of UniquePtr
struct SCATHA(API) AbstractSyntaxTree;

/**
 ***Smart pointer for allocating AST nodes

 Used to have a common interface for allocating nodes in the AST. Should not be  used to allocate other things so we can
 grep for this and perhaps switch to some more efficient allocation strategy in the future.
 */
template <std::derived_from<AbstractSyntaxTree> T>
class UniquePtr: public std::unique_ptr<T> {
    using Base = std::unique_ptr<T>;

public:
    UniquePtr(std::unique_ptr<T>&& p): Base(std::move(p)) {}
    using Base::Base;
};

template <typename T, typename... Args>
requires std::constructible_from<T, Args...> UniquePtr<T> allocate(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

/**
 ***Base class for all nodes in the AST

 Every derived class must specify its runtime type in the constructor via the \p NodeType enum.
 */
struct SCATHA(API) AbstractSyntaxTree {
public:
    virtual ~AbstractSyntaxTree() = default;

    /// Runtime type of this node
    NodeType nodeType() const { return _type; }

    /// Token associated with this node
    Token const& token() const { return _token; }

    /// Source location object associated with this node. Same as
    /// token().sourceLocation
    SourceLocation sourceLocation() const { return token().sourceLocation; }

protected:
    explicit AbstractSyntaxTree(NodeType type, Token const& token): _type(type), _token(token) {}

protected:
    NodeType _type;
    Token _token;
};

} // namespace scatha::ast

#endif // SCATHA_AST_BASE_H_
