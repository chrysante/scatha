#include "SimpleAnalzyer.h"

#include "AST/AST.h"
#include "AST/Expression.h"
#include "Lexer/Lexer.h"
#include "Parser/Parser.h"
#include "Parser/ParsingIssue.h"
#include "Sema/Analyze.h"
#include "Sema/SemanticIssue.h"

namespace scatha::test {

	std::tuple<
		ast::UniquePtr<ast::AbstractSyntaxTree>,
		sema::SymbolTable,
		issue::IssueHandler
	> produceDecoratedASTAndSymTable(std::string_view text) {
		lex::Lexer l(text);
		auto tokens = l.lex();
		
		parse::Parser p(tokens);
		auto ast = p.parse();

		issue::IssueHandler iss;
		
		auto sym = sema::analyze(ast.get(), iss);
		
		return { std::move(ast), std::move(sym), std::move(iss) };
	}
	
}
