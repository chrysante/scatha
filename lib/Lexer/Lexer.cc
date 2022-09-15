#include "Lexer/Lexer.h"

#include "Lexer/LexerUtil.h"
#include "Lexer/LexerError.h"

namespace scatha::lex {

	Lexer::Lexer(std::string_view text):
		text(std::move(text))
	{
		
	}
	
	utl::vector<Token> Lexer::lex() {
		assert(result.empty() && "Lexer has been run before");
		assert(sc.index == 0 && "Lexer has been run before");
		
		size_t const length = text.size();
		
		for (sc.index = 0, sc.line = 1, sc.column = 1; sc.index < length; ++sc.index, ++sc.column) {
			char const c = text[sc.index];
			
			if (lexOneLineComment(c)) {
				continue;
			}
			
			if (isDelimiter(c)) {
				submitCurrentToken();
			}

			if (isSpace(c)) {
				if (isNewline(c)) {
					++sc.line;
					sc.column = 0;
				}
				continue;
			}
			
			if (isPunctuation(c)) {
				beginToken(TokenType::Punctuation);
				currentToken.id += c;
				submitCurrentToken();
				continue;
			}
			
			if (!lexingToken && isLetter(c)) {
				beginToken(TokenType::Identifier);
			}
			
			if (!lexingToken && isDigitDec(c)) {
				beginToken(TokenType::IntegerLiteral);
			}
			
			if (isLetterEx(c) && lexingToken) {
				if (currentToken.type != TokenType::Identifier && currentToken.type != TokenType::IntegerLiteral) {
					throw LexerError(sc, "Unexpected ID");
				}
				currentToken.id += c;
				continue;
			}
			
			if (lexOperator(c)) {
				continue;
			}
			
			if (lexStringLiteral(c)) {
				continue;
			}
			
		}
		
		submitCurrentToken();
		beginToken(TokenType::EndOfFile);
		submitCurrentToken();
		return std::move(result);
	}
	
	void Lexer::beginToken(TokenType type) {
		assert(!lexingToken);
		currentToken.sourceLocation = sc;
		currentToken.type = type;
		lexingToken = true;
	}
	
	void Lexer::submitCurrentToken() {
		if (!lexingToken) {
			return;
		}
		result.push_back(currentToken);
		currentToken = {};
		lexingToken = false;
	}

	bool Lexer::lexOperator(char c) {
		if (!isOperatorLetter(c)) {
			return false;
		}
		
		if ((lexingToken && currentToken.type != TokenType::Operator) || !lexingToken) {
			submitCurrentToken();
			beginToken(TokenType::Operator);
		}
		
		currentToken.id += c;
		
		bool const isOp = isOperator(currentToken.id);
		bool const nextIsOp = sc.index + 1 < text.size() && isOperator(currentToken.id + text[sc.index + 1]);
		
		if (isOp && !nextIsOp) {
			submitCurrentToken();
		}
		
		return true;
	}

	bool Lexer::lexStringLiteral(char c) {
		if (c != '"') {
			return false;
		}
		submitCurrentToken();
		sc.index += 1;
		size_t index = sc.index;
		while (true) {
			if (index == text.size() || text[index] == '\n') {
				throw LexerError(sc, "Unterminated String Literal");
			}
			if (text[index] == '"') {
				break;
			}
			
			++index;
		}
		assert(text[index] == '"');
		
		size_t const length = index - sc.index;
		
		beginToken(TokenType::StringLiteral);
		currentToken.id = text.substr(sc.index, length);
		submitCurrentToken();
		sc.index += length;
		sc.column += length;
		return true;
	}
	
	bool Lexer::lexOneLineComment(char c) {
		if (c != '/') {
			return false;
		}
		std::size_t index = sc.index + 1;
		if (index >= text.size()) {
			return false;
		}
		if (text[index] != '/') {
			return false;
		}
		submitCurrentToken();
		sc.index += 1;
		while (true) {
			if (sc.index == text.size() || text[sc.index] == '\n') {
				sc.line += 1;
				sc.column = 0;
				return true;
			}
			++sc.index;
		}
	}
	
}
