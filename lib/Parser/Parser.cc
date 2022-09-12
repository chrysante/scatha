#include "Parser/Parser.h"

#include "Parser/Keyword.h"
#include "Parser/ParserError.h"

#define with(...) if (__VA_ARGS__; true)

namespace scatha::parse {

	Parser::Parser(std::span<Token const> tokens, Allocator& alloc):
		alloc(alloc),
		inTokens(tokens),
		typeTable(std::make_shared<TypeTable>()),
		functionTable(std::make_shared<FunctionTable>(typeTable))
	{
		
	}
	
	ParseTreeNode* Parser::parse() {
		prepass();
		
		utl::small_vector<ParseTreeNode*> nodes;
		while (true) {
			auto* const node = parseLevelOne();
			if (node == nullptr) {
				break;
			}
			nodes.push_back(node);
		}
		
		RootNode* result = allocate<RootNode>(alloc);
		result->nodes = allocateArray<ParseTreeNode*>(alloc, nodes.begin(), nodes.end());
		return result;
	}
	
	ParseTreeNode* Parser::parseLevelOne() {
		TokenEx const* token = eatToken();
		
		if (token->type == TokenType::EndOfFile) {
			return nullptr;
		}
		
		expectKeyword(*token);
		
		switch (token->keyword) {
				using enum Keyword;
			case Function:
				return parseFunction();
				break;
				
			default:
				throw ParserError(*token, "Expected Declarator");
				break;
		}
		
	}
	
	TokenEx const* Parser::_getTokenImpl(bool ignoreSeparators, size_t* i) {
		if (ignoreSeparators) {
			this->ignoreSeparators(i);
		}
		if (*i == exTokens.size()) { return nullptr; }
		return &exTokens[(*i)++];
	}
	
	TokenEx const* Parser::eatToken(bool ignoreSeparators) {
		return _getTokenImpl(ignoreSeparators, &index);
	}
	
	TokenEx const* Parser::peekToken(bool ignoreSeparators) {
		size_t i = index;
		return _getTokenImpl(ignoreSeparators, &i);
	}
	
	FunctionDeclaration* Parser::parseFunction() {
		FunctionDeclaration* const decl = parseFunctionDecl();
		TokenEx const* token = nullptr;
		while (true) {
			token = peekToken(false);
			if (token->id == ";") {
				eatToken();
				return decl;
			}
			if (token->id == "EOL") {
				eatToken(false);
				continue;
			}
			if (token->id == "{") {
				FunctionDefiniton* def = allocate<FunctionDefiniton>(alloc, *decl);
				def->body = parseBlock();
				return def;
			}
			throw ParserError(*token, "Unexpected ID");
		};
	}
	
	FunctionDeclaration* Parser::parseFunctionDecl() {
		state = State::ParsingFunctionDecl;
		
		auto const* name = eatToken();
		expectIdentifier(*name);
		
		auto* result = allocate<FunctionDeclaration>(alloc);
		result->name = name->id;
		
		parseFunctionParameters(result);
		
		auto const* token = peekToken();
		if (token->id == "->") {
			eatToken();
			token = eatToken();
			expectIdentifier(*token);
			result->returnType = token->id;
		}
		else {
			result->returnType = "void";
		}
		
		return result;

	}

	void Parser::parseFunctionParameters(FunctionDeclaration* fn) {
		auto next = [this]() -> auto* { return eatToken(false); };
		auto const* token = next();
		
		expectID(*token, "(");
		
		if (auto const* const token = peekToken(); token && token->id == ")") {
			eatToken();
			return;
		}
		
		utl::small_vector<FunctionParameterDecl, 8> params;
		
		while (true) {
			token = next();
			while (token->id == "EOL") {
				token = next();
			}
			
			expectIdentifier(*token);
			std::string name = token->id;
			token = next();
			expectID(*token, ":");
			token = next();
			expectIdentifier(*token);
			std::string type = token->id;
			
			params.push_back({ std::move(name), std::move(type) });
			
			token = next();
			if (token->id != ",") {
				expectID(*token, ")");
				break;
			}
		}
		
		fn->params = allocateArray<FunctionParameterDecl>(alloc, params.begin(), params.end());
	}
	
	Block* Parser::parseBlock() {
		TokenEx const* token = eatToken();
		
		expectID(*token, "{");
		
		utl::small_vector<Statement*> statements;
		
		while (true) {
			token = peekToken();
			if (token->id == "}") {
				eatToken();
				break;
			}
			statements.push_back(parseStatement());
		}
		
		Block* const result = allocate<Block>(alloc);
		result->statements = allocateArray<Statement*>(alloc, statements.begin(), statements.end());
		return result;
	}
	
	Statement* Parser::parseStatement() {
		TokenEx const* token = eatToken();
		if (token->isKeyword) {
			using enum Keyword;
			switch (token->keyword) {
				case Var: [[fallthrough]];
				case Let:
					return parseVariableDeclaration(*token);
					
				case Return:
					return parseReturnStatement();
					
				default:
					throw ParserError(*token, "Unexpected ID");
			}
		}
		return nullptr;
	}
	
	VariableDeclaration* Parser::parseVariableDeclaration(TokenEx const& declarator) {
		assert(declarator.isKeyword && (declarator.keyword == Keyword::Var || declarator.keyword == Keyword::Let));
		
		VariableDeclaration* result = allocate<VariableDeclaration>(alloc);
		
		TokenEx const* token = eatToken();
		expectIdentifier(*token);
		
		result->name = token->id;
		result->isConstant = declarator.keyword == Keyword::Let;
		
		token = eatToken();
		
		if (token->id == ":") {
			token = eatToken();
			expectIdentifier(*token);
			result->type = token->id;
			token = eatToken();
		}
		
		if (token->id == "=") {
			result->initExpression = parseExpression();
			token = eatToken(false);
		}
		
		return result;
	}
	
	ReturnStatement* Parser::parseReturnStatement() {
		ReturnStatement* result = allocate<ReturnStatement>(alloc);
		
		TokenEx const* token = eatToken();
#warning Parse the expression instead of doing this
		expectIdentifier(*token); // for now
		
		result->expression = allocate<Expression>(alloc);
		
		return result;
	}
	
	Expression* Parser::parseExpression() {
		return allocate<Expression>(alloc);
	}
	
	void Parser::prepass() {
		exTokens.reserve(inTokens.size());
		for (auto& token: inTokens) {
			exTokens.push_back(expand(token));
		}
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
		
	void Parser::ignoreSeparators(size_t* i) const {
		while (*i < exTokens.size() && exTokens[*i].isSeparator) { ++*i; }
	}
	
}
