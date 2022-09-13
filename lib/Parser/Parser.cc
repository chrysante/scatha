#include "Parser/Parser.h"

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
			auto node = parseRootLevel();
			if (node == nullptr) {
				break;
			}
			result->nodes.push_back(std::move(node));
		}
		
		return result;
	}
	
	/// MARK: Private
	ast::UniquePtr<ast::AbstractSyntaxTree> Parser::parseRootLevel() {
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
			ast::UniquePtr<ast::FunctionDefiniton> def = ast::allocate<ast::FunctionDefiniton>(*decl);
			def->body = parseBlock();
			return def;
		}
		throw ParserError(token, "Unexpected ID");
	}
	
	ast::UniquePtr<ast::FunctionDeclaration> Parser::parseFunctionDecl() {
		TokenEx const& name = tokens.eat();
		expectIdentifier(name);
		
		auto result = ast::allocate<ast::FunctionDeclaration>(name.id);
		
		parseFunctionParameters(result.get());
		
		if (TokenEx const& token = tokens.peek(); token.id == "->") {
			tokens.eat();
			TokenEx const& type = tokens.eat();
			expectIdentifier(type);
			result->returnType = type.id;
		}
		else {
			result->returnType = "void";
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
			TokenEx const& name = tokens.eat();
			expectIdentifier(name);
			
			expectID(tokens.eat(), ":");
			
			TokenEx const& type = tokens.eat();
			expectIdentifier(type);
			
			fn->params.push_back({ name.id, type.id });
			
			if (TokenEx const& next = tokens.eat(); next.id != ",") {
				expectID(next, ")");
				break;
			}
		}
	}
	
	ast::UniquePtr<ast::Block> Parser::parseBlock() {
		expectID(tokens.eat(), "{");
		
		auto result = ast::allocate<ast::Block>();
		
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
		ast::UniquePtr<ast::Statement> result = nullptr;
		if (token.isKeyword) {
			using enum Keyword;
			tokens.eat();
			switch (token.keyword) {
				case Var: [[fallthrough]];
				case Let:
					result = parseVariableDeclaration(token);
					break;
					
				case Return:
					result = parseReturnStatement();
					break;
					
				default:
					throw ParserError(token, "Unexpected ID");
			}
		}
		else {
			// We have not eaten the first token yet. Parsing the expression should be fine.
			result = ast::allocate<ast::ExpressionStatement>(parseExpression());
		}
		if (result == nullptr) {
			throw ParserError(token, "Can't handle this statement.");
		}
		TokenEx const& next = tokens.eat(false);
		expectSeparator(next);
		return result;
	}
	
	ast::UniquePtr<ast::VariableDeclaration> Parser::parseVariableDeclaration(TokenEx const& declarator) {
		assert(declarator.isKeyword && (declarator.keyword == Keyword::Var || declarator.keyword == Keyword::Let));
		
		
		
		TokenEx const& name = tokens.eat();
		expectIdentifier(name);
		
		auto result = ast::allocate<ast::VariableDeclaration>(name.id);
		result->isConstant = declarator.keyword == Keyword::Let;
		
		if (tokens.peek().id == ":") {
			tokens.eat();
			TokenEx const& type = tokens.eat();
			expectIdentifier(type);
			result->type = type.id;
		}
		
		if (tokens.peek().id == "=") {
			tokens.eat();
			result->initExpression = parseExpression();
		}
		
		return result;
	}
	
	ast::UniquePtr<ast::ReturnStatement> Parser::parseReturnStatement() {
		return ast::allocate<ast::ReturnStatement>(parseExpression());
	}
	
	ast::UniquePtr<ast::Expression> Parser::parseExpression() {
		ExpressionParser parser(tokens);
		return parser.parseExpression();
	}
	
	void Parser::expectIdentifier(TokenEx const& token) {
		if (!token.isIdentifier) {
			throw ParserError(token, "Expected Identifier");
		}
	}
	
	void Parser::expectKeyword(TokenEx const& token) {
		if (!token.isKeyword) {
			throw ParserError(token, "Expected Keyword");
		}
	}
	
	void Parser::expectDeclarator(TokenEx const& token) {
		if (!token.isKeyword || token.keywordCategory != KeywordCategory::Declarators) {
			throw ParserError(token, "Expected Declarator");
		}
	}
	
	void Parser::expectSeparator(TokenEx const& token) {
		if (!token.isSeparator) {
			throw ParserError(token, "Unqualified ID");
		}
	}
	
	void Parser::expectID(TokenEx const& token, std::string_view id) {
		if (token.id != id) {
			throw ParserError(token, "Expected '" + std::string(id) + "'");
		}
	}
	
}
