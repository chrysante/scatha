#include "Parser/Parser.h"

#include "Parser/Keyword.h"
#include "Parser/ParserError.h"

#define with(...) if (__VA_ARGS__; true)

namespace scatha::parse {

	/// MARK: Public
	Parser::Parser(std::span<Token const> tokens, Allocator& alloc):
		alloc(alloc),
		tokens(tokens)
	{
		
	}
	
	ParseTreeNode* Parser::parse() {
		utl::small_vector<ParseTreeNode*> nodes;
		while (true) {
			auto* const node = parseRootLevel();
			if (node == nullptr) {
				break;
			}
			nodes.push_back(node);
		}
		
		RootNode* result = allocate<RootNode>(alloc);
		result->nodes = allocateArray<ParseTreeNode*>(alloc, nodes.begin(), nodes.end());
		return result;
	}
	
	/// MARK: Private
	ParseTreeNode* Parser::parseRootLevel() {
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
	
	FunctionDeclaration* Parser::parseFunction() {
		FunctionDeclaration* const decl = parseFunctionDecl();
	
		TokenEx const& token = tokens.peek();
		if (token.isSeparator) {
			tokens.eat();
			return decl;
		}
		if (token.id == "{") {
			FunctionDefiniton* def = allocate<FunctionDefiniton>(alloc, *decl);
			def->body = parseBlock();
			return def;
		}
		throw ParserError(token, "Unexpected ID");
	}
	
	FunctionDeclaration* Parser::parseFunctionDecl() {
		TokenEx const& name = tokens.eat();
		expectIdentifier(name);
		
		auto* result = allocate<FunctionDeclaration>(alloc);
		result->name = name.id;
		
		parseFunctionParameters(result);
		
		
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

	void Parser::parseFunctionParameters(FunctionDeclaration* fn) {
		TokenEx const& openParan = tokens.eat();
		
		expectID(openParan, "(");
		
		if (tokens.peek().id == ")") {
			tokens.eat();
			return;
		}
		
		utl::small_vector<FunctionParameterDecl, 8> params;
		
		while (true) {
			TokenEx const& name = tokens.eat();
			expectIdentifier(name);
			
			expectID(tokens.eat(), ":");
			
			TokenEx const& type = tokens.eat();
			expectIdentifier(type);
			
			params.push_back({ name.id, type.id });
			
			if (TokenEx const& next = tokens.eat(); next.id != ",") {
				expectID(next, ")");
				break;
			}
		}
		
		fn->params = allocateArray<FunctionParameterDecl>(alloc, params.begin(), params.end());
	}
	
	Block* Parser::parseBlock() {
		expectID(tokens.eat(), "{");
		
		utl::small_vector<Statement*> statements;
		
		while (true) {
			if (tokens.peek().id == "}") {
				tokens.eat();
				break;
			}
			statements.push_back(parseStatement());
		}
		
		Block* const result = allocate<Block>(alloc);
		result->statements = allocateArray<Statement*>(alloc, statements.begin(), statements.end());
		return result;
	}
	
	Statement* Parser::parseStatement() {
		TokenEx const& token = tokens.eat();
		Statement* result = nullptr;
		if (token.isKeyword) {
			using enum Keyword;
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
		if (result == nullptr) {
			throw ParserError(token, "Can't handle this statement.");
		}
		TokenEx const& next = tokens.eat(false);
		expectSeparator(next);
		return result;
	}
	
	VariableDeclaration* Parser::parseVariableDeclaration(TokenEx const& declarator) {
		assert(declarator.isKeyword && (declarator.keyword == Keyword::Var || declarator.keyword == Keyword::Let));
		
		VariableDeclaration* result = allocate<VariableDeclaration>(alloc);
		
		TokenEx const& name = tokens.eat();
		expectIdentifier(name);
		
		result->name = name.id;
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
	
	ReturnStatement* Parser::parseReturnStatement() {
		ReturnStatement* result = allocate<ReturnStatement>(alloc);
		result->expression = parseExpression();
		return result;
	}
	
	Expression* Parser::parseExpression() {
#warning right now we expect an expression to be exactly one identifier or literal
		TokenEx const& token = tokens.eat();
		if (token.type == TokenType::Identifier ||
			token.type == TokenType::NumericLiteral ||
			token.type == TokenType::StringLiteral)
		{
			return allocate<Expression>(alloc);
		}
		throw ParserError(token, "Can't parse this expression yet");
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
