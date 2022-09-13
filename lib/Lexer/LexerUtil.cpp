#include "Lexer/LexerUtil.h"

namespace scatha::lex {
	
	static bool isAnyOf(char c, char const* data) {
		for (char const* i = data; *i; ++i) {
			if (c == *i) {
				return true;
			}
		}
		return false;
	}
	
	SCATHA(PURE) bool isPunctuation(char c) {
		return isAnyOf(c, "{}()[],;:");
	}
	
	SCATHA(PURE) bool isOperatorLetter(char c) {
		return isAnyOf(c, "+-*/%&|^.=><?!~");
	}
	
	SCATHA(PURE) bool isOperator(std::string_view id) {
		std::string_view constexpr operators[] {
			"+", "-", "*", "/", "%", "&", "|", "^",
			"!", "~",
			"<<", ">>",
			"&&", "||",
			
			"=",
			"+=", "-=", "*=", "/=", "%=",
			"<<=", ">>=",
			"&=", "|=", "^=",
			
			"==", "!=", "<", "<=", ">", ">=",
			
			".", "->"
			/// TODO: Add other operators
		};
		for (auto o: operators) {
			if (id == o) {
				return true;
			}
		}
		return false;
	}
	
}
