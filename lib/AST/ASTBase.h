#pragma once

#ifndef SCATHA_AST_ASTBASE_H_
#define SCATHA_AST_ASTBASE_H_

#include <memory>
#include <string>

#include "AST/NodeType.h"
#include "Common/Token.h"
#include "Common/SourceLocation.h"

namespace scatha::ast {
	
	template <typename T>
	using UniquePtr = std::unique_ptr<T>;
	
	template <typename T, typename... Args> requires std::constructible_from<T, Args...>
	UniquePtr<T> allocate(Args&&... args) {
		return std::make_unique<T>(std::forward<Args>(args)...);
	}
	
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

#endif // SCATHA_AST_ASTBASE_H_
