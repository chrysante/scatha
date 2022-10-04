#pragma once

#ifndef SCATHA_COMMON_KEYWORD_H_
#define SCATHA_COMMON_KEYWORD_H_

#include <optional>
#include <string_view>

#include "Basic/Basic.h"

namespace scatha {
	
	enum class Keyword: u8 {
		Void, Bool, Int, Float, String,
		
		Import, Export,
		
		Module, Class, Struct, Function, Var, Let,
		
		Return, If, Else, For, While, Do,
		
		False, True,
		
		Public, Protected, Private,
		
		Placeholder,
		
		_count
	};
	
	enum class KeywordCategory: u8 {
		Types,
		Modules,
		Declarators,
		ControlFlow,
		BooleanLiterals,
		AccessSpecifiers,
		Placeholder
	};
	
	SCATHA(API) std::optional<Keyword> toKeyword(std::string_view);
	
	SCATHA(API) bool isDeclarator(Keyword);
	
	SCATHA(API) bool isControlFlow(Keyword);
	
	SCATHA(API) KeywordCategory categorize(Keyword);
	
}

#endif // SCATHA_LEXER_COMMON_H_
