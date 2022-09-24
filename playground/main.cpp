#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include <sstream>

#include "AST/PrintSource.h"
#include "AST/PrintTree.h"
#include "AST/Traversal.h"

#include "IC/TACGenerator.h"
#include "IC/Canonicalize.h"
#include "IC/PrintTAC.h"
#include "Lexer/Lexer.h"
#include "Parser/Parser.h"
#include "Parser/ExpressionParser.h"
#include "Sema/SemanticAnalyzer.h"
#include "Sema/PrintSymbolTable.h"

using namespace scatha;
using namespace scatha::lex;
using namespace scatha::parse;


__attribute__((weak)) int main() {
	
	auto const filepath = std::filesystem::path(PROJECT_LOCATION) / "playground/Test.sc";
	std::fstream file(filepath);
	if (!file) {
		std::cout << "Failed to open file " << filepath << std::endl;
	}
	
	std::stringstream sstr;
	sstr << file.rdbuf();
	std::string const text = sstr.str();
	
	try {
		Lexer l(text);
		auto tokens = l.lex();
		
		Parser p(tokens);
		auto ast = p.parse();
		
		sema::SemanticAnalyzer s;
		s.run(ast.get());
		
		ic::canonicalize(ast.get());
		
		ic::TACGenerator t(s.symbolTable());
		auto const tac = t.run(ast.get());
		
		ic::printTAC(tac, s.symbolTable());
	}
	catch (std::exception const& e) {
		std::cout << e.what() << std::endl;
	}
	
}

