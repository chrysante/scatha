#pragma once

#ifndef SCATHA_PARSER_KEYWORD_H_
#define SCATHA_PARSER_KEYWORD_H_

#include <optional>

#include "Basic/Basic.h"

namespace scatha {
	
	struct Token;

}

namespace scatha::parse {
	
	enum class Keyword {
		Void, Bool, Int, Float, String,
		
		Import, Export,
		
		Module, Class, Struct, Function, Var, Let,
		
		Return, If, Else, For, While, Do,
		
		False, True,
		
		Public, Protected, Private,
		
		Placeholder,
		
		_count
	};
	
	enum class KeywordCategory {
		Types,
		Modules,
		Declarators,
		ControlFlow,
		BooleanLiterals,
		AccessSpecifiers,
		Placeholder
	};
	
	SCATHA(API) bool isDeclarator(Keyword);
	
	SCATHA(API) bool isControlFlow(Keyword);
	
	SCATHA(API) std::optional<Keyword> toKeyword(Token const&);
	
	SCATHA(API) KeywordCategory categorize(Keyword);
	
}

#endif // SCATHA_LEXER_KEYWORD_H_
