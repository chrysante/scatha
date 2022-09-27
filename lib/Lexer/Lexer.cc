#include "Lexer/Lexer.h"

#include "Lexer/LexerUtil.h"
#include "Lexer/LexicalIssue.h"

namespace scatha::lex {

	Lexer::Lexer(std::string_view text):
		text(std::move(text))
	{
		
	}
	
	utl::vector<Token> Lexer::lex() {
		SC_ASSERT(currentLocation.index == 0, "Lexer has been run before");
		
		utl::vector<Token> result;
		
		while (true) {
			if (auto token = getToken()) {
				result.push_back(std::move(*token));
				continue;
			}
			if (currentLocation.index >= text.size()) {
				SC_ASSERT(currentLocation.index == text.size(), "How is this possible?");
				
				Token eof = beginToken2(TokenType::EndOfFile);
				result.push_back(eof);
				return result;
			}
			throw UnexpectedID(beginToken2(TokenType::Other));
		}
	}
	
	std::optional<Token> Lexer::getToken() {
		if (auto spaces = getSpaces()) {
			return getToken();
		}
		if (auto comment = getOneLineComment()) {
			return getToken();
		}
		if (auto comment = getMultiLineComment()) {
			return getToken();
		}
		if (auto punctuation = getPunctuation()) {
			return *punctuation;
		}
		if (auto op = getOperator()) {
			return *op;
		}
		if (auto integerLiteral = getIntegerLiteral()) {
			return *integerLiteral;
		}
		if (auto integerLiteralHex = getIntegerLiteralHex()) {
			return *integerLiteralHex;
		}
		if (auto floatingPointLiteral = getFloatingPointLiteral()) {
			return *floatingPointLiteral;
		}
		if (auto stringLiteral = getStringLiteral()) {
			return *stringLiteral;
		}
		if (auto booleanLiteral = getBooleanLiteral()) {
			return *booleanLiteral;
		}
		if (auto identifier = getIdentifier()) {
			return *identifier;
		}
		
		return std::nullopt;
	}
	
	std::optional<Token> Lexer::getSpaces() {
		if (!isSpace(current())) {
			return std::nullopt;
		}
		Token result = beginToken2(TokenType::Other);
		while (isSpace(current())) {
			result.id += current();
			if (!advance()) {
				break;
			}
		}
		return result;
	}
	
	std::optional<Token> Lexer::getOneLineComment() {
		if (current() != '/') {
			return std::nullopt;
		}
		if (auto const next = this->next();
			!next || *next != '/')
		{
			return std::nullopt;
		}
		Token result = beginToken2(TokenType::Other);
		result.id += current();
		advance();
		while (true) {
			result.id += current();
			if (current() == '\n' || !advance()) {
				return result;
			}
		}
	}
	
	std::optional<Token> Lexer::getMultiLineComment() {
		if (current() != '/') {
			return std::nullopt;
		}
		if (auto const next = this->next();
			!next || *next != '*')
		{
			return std::nullopt;
		}
		Token result = beginToken2(TokenType::Other);
		result.id += current();
		advance();
		// now we are at the next character after "/*"
		while (true) {
			result.id += current();
			
			if (!advance()) {
				throw UnterminatedMultiLineComment(result);
			}
			
			if (result.id.back() == '*' && current() == '/') {
				result.id += current();
				advance();
				return result;
			}
		}
	}
	
	std::optional<Token> Lexer::getPunctuation() {
		if (!isPunctuation(current())) {
			return std::nullopt;
		}
		Token result = beginToken2(TokenType::Punctuation);
		result.id += current();
		advance();
		return result;
	}
	
	std::optional<Token> Lexer::getOperator() {
		Token result = beginToken2(TokenType::Operator);
		result.id += current();
		
		if (!isOperator(result.id)) {
			return std::nullopt;
		}
		
		while (true) {
			if (!advance()) {
				return result;
			}
			result.id += current();
			if (!isOperator(result.id)) {
				result.id.pop_back();
				return result;
			}
		}
	}
	
	std::optional<Token> Lexer::getIntegerLiteral() {
		if (!isDigitDec(current())) {
			return std::nullopt;
		}
		if (current() == '0' && next() && *next() == 'x') {
			return std::nullopt; // We are a hex literal, not our job
		}
		Token result = beginToken2(TokenType::IntegerLiteral);
		result.id += current();
		size_t offset = 1;
		std::optional next = this->next(offset);
		while (next && isDigitDec(*next)) {
			result.id += *next;
			++offset;
			next = this->next(offset);
		}
		if (!next || isDelimiter(*next)) {
			while (offset-- > 0) {
				advance();
			}
			return result;
		}
		if (*next == '.') {
			// we are a floating point literal
			return std::nullopt;
		}
		throw InvalidNumericLiteral(result);
	}
	
	std::optional<Token> Lexer::getIntegerLiteralHex() {
		if (current() != '0' || !next() || *next() != 'x') {
			return std::nullopt;
		}
		Token result = beginToken2(TokenType::IntegerLiteral);
		result.id += current();
		advance();
		result.id += current();
		// Now result.id == "0x"
		while (advance() && isDigitHex(current())) {
			result.id += current();
		}
		if (next() && !isLetter(*next())) {
			return result;
		}
		throw InvalidNumericLiteral(result);
	}
	
	std::optional<Token> Lexer::getFloatingPointLiteral() {
		if (!isFloatDigitDec(current())) {
			return std::nullopt;
		}
		Token result = beginToken2(TokenType::FloatingPointLiteral);
		result.id += current();
		size_t offset = 1;
		std::optional next = this->next(offset);
		while (next && isFloatDigitDec(*next)) {
			result.id += *next;
			next = this->next(++offset);
		}
		if (result.id == ".") { // this is not a floating point literal
			return std::nullopt;
		}
		if (!next || isDelimiter(*next)) {
			while (offset-- > 0) {
				advance();
			}
			return result;
		}
		throw InvalidNumericLiteral(result);
	}
	std::optional<Token> Lexer::getStringLiteral() {
		if (current() != '"') {
			return std::nullopt;
		}
		Token result = beginToken2(TokenType::StringLiteral);
		if (!advance()) {
			throw UnterminatedStringLiteral(result);
		}
		while (true) {
			result.id += current();
			if (!advance() || current() == '\n') {
				throw UnterminatedStringLiteral(result);
			}
			if (current() == '"') {
				advance();
				return result;
			}
		}
	}
	
	std::optional<Token> Lexer::getBooleanLiteral() {
		if (currentLocation.index + 3 < text.size() &&
			text.substr(currentLocation.index, 4) == "true")
		{
			if (auto const n = next(4); n && isLetterEx(*n)) {
				return std::nullopt;
			}
			Token result = beginToken2(TokenType::BooleanLiteral);
			result.id = "true";
			advance(4);
			return result;
		}
		if (currentLocation.index + 4 < text.size() &&
			text.substr(currentLocation.index, 5) == "false")
		{
			if (auto const n = next(5); n && isLetterEx(*n)) {
				return std::nullopt;
			}
			Token result = beginToken2(TokenType::BooleanLiteral);
			result.id = "false";
			advance(5);
			return result;
		}
		return std::nullopt;
	}
	
	std::optional<Token> Lexer::getIdentifier() {
		if (!isLetter(current())) {
			return std::nullopt;
		}
		Token result = beginToken2(TokenType::Identifier);
		result.id += current();
		while (advance() && isLetterEx(current())) {
			result.id += current();
		}
		return result;
	}
	
	bool Lexer::advance() {
		if (text[currentLocation.index] == '\n') {
			currentLocation.column = 0;
			++currentLocation.line;
		}
		++currentLocation.index;
		++currentLocation.column;
		if (currentLocation.index == text.size()) {
			return false;
		}
		return true;
	}
	
	bool Lexer::advance(size_t count) {
		while (count-- > 0) {
			if (!advance()) {
				return false;
			}
		}
		return true;
	}
	
	Token Lexer::beginToken2(TokenType type) const {
		Token result;
		result.sourceLocation = currentLocation;
		result.type = type;
		return result;
	}
	
	char Lexer::current() const {
		return text[currentLocation.index];
	}
	
	std::optional<char> Lexer::next(size_t offset) const {
		if (currentLocation.index + offset >= text.size()) {
			return std::nullopt;
		}
		return text[currentLocation.index + offset];
	}
	
}
