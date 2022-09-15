#include "Parser/Parser.h"

#include "Basic/Basic.h"

#include "Parser/ExpressionParser.h"
#include "Parser/Keyword.h"
#include "Parser/ParserError.h"

namespace scatha::parse {

	/// MARK: Public
	Parser::Parser(std::span<Token const> tokens):
		tokens(tokens)
	{
		
	}
	
	ast::UniquePtr<ast::AbstractSyntaxTree> Parser::parse() {
		ast::UniquePtr<ast::TranslationUnit> result = ast::allocate<ast::TranslationUnit>();
		
		while (true) {
			auto decl = parseRootLevelDeclaration();
			if (decl == nullptr) {
				break;
			}
			result->declarations.push_back(std::move(decl));
		}
		
		return result;
	}
	
	/// MARK: Private
	ast::UniquePtr<ast::AbstractSyntaxTree> Parser::parseRootLevelDeclaration() {
		TokenEx const& token = tokens.eat();
		
		if (token.type == TokenType::EndOfFile) {
			return nullptr;
		}
		
		expectKeyword(token);
		
		switch (token.keyword) {
				using enum Keyword;
			case Function:
				return parseFunction();
				break;
				
			default:
				throw ParserError(token, "Expected Declarator");
				break;
		}
	}
	
	ast::UniquePtr<ast::FunctionDeclaration> Parser::parseFunction() {
		auto decl = parseFunctionDecl();
	
		TokenEx const& token = tokens.peek();
		if (token.isSeparator) {
			tokens.eat();
			return decl;
		}
		if (token.id == "{") {
			ast::UniquePtr<ast::FunctionDefinition> def = ast::allocate<ast::FunctionDefinition>(std::move(*decl));
			def->body = parseBlock();
			return def;
		}
		throw ParserError(token, "Unexpected ID");
	}
	
	ast::UniquePtr<ast::FunctionDeclaration> Parser::parseFunctionDecl() {
		TokenEx const& name = tokens.eat();
		expectIdentifier(name);
		
		auto result = ast::allocate<ast::FunctionDeclaration>(name);
		
		parseFunctionParameters(result.get());
		
		if (TokenEx const& token = tokens.peek(); token.id == "->") {
			tokens.eat();
			TokenEx const& type = tokens.eat();
			expectIdentifier(type);
			result->declReturnTypename = type;
		}
		else {
			auto copy = token;
			copy.id = "void";
			copy.type = TokenType::Identifier;
			result->declReturnTypename = copy;
		}
		
		return result;

	}

	void Parser::parseFunctionParameters(ast::FunctionDeclaration* fn) {
		TokenEx const& openParan = tokens.eat();
		
		expectID(openParan, "(");
		
		if (tokens.peek().id == ")") {
			tokens.eat();
			return;
		}
		
		while (true) {
			fn->params.push_back(parseVariableDeclaration(/* requireTypename = */ true));
			
			if (TokenEx const& next = tokens.eat(); next.id != ",") {
				expectID(next, ")");
				break;
			}
		}
	}
	
	ast::UniquePtr<ast::Block> Parser::parseBlock() {
		auto const& openBrace = tokens.eat();
		expectID(openBrace, "{");
		
		auto result = ast::allocate<ast::Block>(openBrace);
		
		while (true) {
			if (tokens.peek().id == "}") {
				tokens.eat();
				break;
			}
			result->statements.push_back(parseStatement());
		}
		
		return result;
	}
	
	ast::UniquePtr<ast::Statement> Parser::parseStatement() {
		TokenEx const& token = tokens.peek();
		if (token.isKeyword) {
			using enum Keyword;
			tokens.eat();
			switch (token.keyword) {
				case Var: [[fallthrough]];
				case Let:
					return parseVariableDeclaration();
					
				case Return:
					return parseReturnStatement();
					
				case If:
					return parseIfStatement();
					
				case While:
					return parseWhileStatement();
					
				default:
					throw ParserError(token, "Unexpected ID");
			}
		}
		else if (token.id == "{") {
			return parseBlock();
		}
		else {
			// We have not eaten the first token yet. Parsing an expression should be fine.
			auto result = ast::allocate<ast::ExpressionStatement>(parseExpression(), token);
			TokenEx const& next = tokens.eat(false);
			expectSeparator(next);
			return result;
		}
	}
	
	ast::UniquePtr<ast::VariableDeclaration> Parser::parseVariableDeclaration(bool isFunctionParameter) {
		bool isConst = false;
		if (!isFunctionParameter) {
			auto const& decl = tokens.current();
			SC_ASSERT_AUDIT(decl.isKeyword, "We should have checked this outside of this function");
			SC_ASSERT_AUDIT(decl.keyword == Keyword::Var || decl.keyword == Keyword::Let, "Same here");
			isConst = decl.keyword == Keyword::Let;
		}
		
		TokenEx const& name = tokens.eat();
		expectIdentifier(name);
		
		auto result = ast::allocate<ast::VariableDeclaration>(name);
		result->isConstant = isConst;
		
		if (tokens.peek().id == ":") {
			tokens.eat();
			TokenEx const& type = tokens.eat();
			expectIdentifier(type);
			result->declTypename = type.id;
		}
		else if (isFunctionParameter) {
			expectID(tokens.peek(), ":");
		}
		
		if (tokens.peek().id == "=") {
			if (isFunctionParameter) { throw ParserError(tokens.peek(), "Unqualified ID"); }
			tokens.eat();
			result->initExpression = parseExpression();
		}
		
		if (!isFunctionParameter) {
			TokenEx const& next = tokens.eat(false);
			expectSeparator(next);
		}
		
		return result;
	}
	
	ast::UniquePtr<ast::ReturnStatement> Parser::parseReturnStatement() {
		SC_ASSERT_AUDIT(tokens.current().id == "return", "");
		auto result = ast::allocate<ast::ReturnStatement>(parseExpression(), tokens.current());
		TokenEx const& next = tokens.eat(false);
		expectSeparator(next);
		return result;
	}
	
	ast::UniquePtr<ast::IfStatement> Parser::parseIfStatement() {
		auto const& keyword = tokens.current();
		SC_ASSERT_AUDIT(keyword.isKeyword, "");
		SC_ASSERT_AUDIT(keyword.keyword == Keyword::If, "");
		
		auto condition = parseExpression();
		auto result = ast::allocate<ast::IfStatement>(std::move(condition), keyword);
		
		result->ifBlock = parseBlock();
		auto const& next = tokens.peek();
		if (!next.isKeyword || next.keyword != Keyword::Else) {
			return result;
		}
		tokens.eat();
		result->elseBlock = parseBlock();
		return result;
	}
	
	ast::UniquePtr<ast::WhileStatement> Parser::parseWhileStatement() {
		auto const& keyword = tokens.current();
		SC_ASSERT(keyword.isKeyword, "");
		SC_ASSERT(keyword.keyword == Keyword::While, "");
		
		auto condition = parseExpression();
		auto result = ast::allocate<ast::WhileStatement>(std::move(condition), keyword);
		result->block = parseBlock();
		return result;
	}
	
	ast::UniquePtr<ast::Expression> Parser::parseExpression() {
		ExpressionParser parser(tokens);
		return parser.parseExpression();
	}
	
}
