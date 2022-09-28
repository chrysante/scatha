#include "SimpleAnalzyer.h"

#include "AST/AST.h"
#include "AST/Expression.h"
#include "Lexer/Lexer.h"
#include "Parser/Parser.h"
#include "Parser/ParsingIssue.h"
#include "Sema/SemanticAnalyzer.h"
#include "Sema/SemanticIssue.h"


namespace scatha::test {

	std::tuple<
		ast::UniquePtr<ast::AbstractSyntaxTree>,
		sema::SymbolTable
	> produceDecoratedASTAndSymTable(std::string_view text) {
		lex::Lexer l(text);
		auto tokens = l.lex();
		
		parse::Parser p(tokens);
		auto ast = p.parse();

		sema::SemanticAnalyzer s;
		s.run(ast.get());
		
		return { std::move(ast), s.takeSymbolTable() };
	}
	
}
