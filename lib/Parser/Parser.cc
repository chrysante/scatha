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
			ast::UniquePtr<ast::FunctionDefinition> def = ast::allocate<ast::FunctionDefinition>(*decl);
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
			result->returnTypename = type.id;
		}
		else {
			result->returnTypename = "void";
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
		if (token.isKeyword) {
			using enum Keyword;
			tokens.eat();
			switch (token.keyword) {
				case Var: [[fallthrough]];
				case Let:
					return parseVariableDeclaration(token);
					
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
			auto result = ast::allocate<ast::ExpressionStatement>(parseExpression());
			TokenEx const& next = tokens.eat(false);
			expectSeparator(next);
			return result;
		}
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
		
		TokenEx const& next = tokens.eat(false);
		expectSeparator(next);
		
		return result;
	}
	
	ast::UniquePtr<ast::ReturnStatement> Parser::parseReturnStatement() {
		auto result = ast::allocate<ast::ReturnStatement>(parseExpression());
		TokenEx const& next = tokens.eat(false);
		expectSeparator(next);
		return result;
	}
	
	ast::UniquePtr<ast::IfStatement> Parser::parseIfStatement() {
		auto const& keyword = tokens.current();
		assert(keyword.isKeyword);
		assert(keyword.keyword == Keyword::If);
		
		auto condition = parseExpression();
		auto ifBlock = parseBlock();
		auto result = ast::allocate<ast::IfStatement>(std::move(condition), std::move(ifBlock));
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
		assert(keyword.isKeyword);
		assert(keyword.keyword == Keyword::While);
		
		auto condition = parseExpression();
		auto block = parseBlock();
		return ast::allocate<ast::WhileStatement>(std::move(condition), std::move(block));
	}
	
	ast::UniquePtr<ast::Expression> Parser::parseExpression() {
		ExpressionParser parser(tokens);
		return parser.parseExpression();
	}
	
}
