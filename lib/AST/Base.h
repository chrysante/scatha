#pragma once

#ifndef SCATHA_AST_BASE_H_
#define SCATHA_AST_BASE_H_

#include <memory>
#include <string>

#include "AST/Common.h"
#include "Common/Token.h"
#include "Common/SourceLocation.h"

namespace scatha::ast {
	
	/**
	 Used to have a common interface for allocating nodes in the AST. Should not be used to allocate other things so we can grep for this and perhaps switch to some more efficient allocation strategy in the future.
	 */
	template <std::derived_from<struct AbstractSyntaxTree> T>
	class UniquePtr: public std::unique_ptr<T> {
		using Base = std::unique_ptr<T>;
	public:
		UniquePtr(std::unique_ptr<T>&& p): Base(std::move(p)) {}
		using Base::Base;
	};
	
	template <typename T, typename... Args> requires std::constructible_from<T, Args...>
	UniquePtr<T> allocate(Args&&... args) {
		return std::make_unique<T>(std::forward<Args>(args)...);
	}
	
	/**
	 Base class for all nodes in the AST
	 
	 Every derived class must specify its runtime type in the constructor via the \p NodeType enum.
	 */
	struct AbstractSyntaxTree {
	public:
		virtual ~AbstractSyntaxTree() = default;
		NodeType nodeType() const { return _type; }
		SourceLocation sourceLocation() const { return _token.sourceLocation; }
		Token const& token() const { return _token; }
		
	protected:
		explicit AbstractSyntaxTree(NodeType type, Token const& token):
			_type(type),
			_token(token)
		{}
		
	private:
		friend class FunctionDefinition;
		NodeType _type;
		Token _token;
	};


}

#endif // SCATHA_AST_BASE_H_
