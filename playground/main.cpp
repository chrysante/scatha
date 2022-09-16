#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include <sstream>

#include "AST/PrintSource.h"
#include "AST/PrintTree.h"

#include "Lexer/Lexer.h"
#include "Parser/Parser.h"
#include "SemanticAnalyzer/SemanticAnalyzer.h"
#include "SemanticAnalyzer/PrintSymbolTable.h"

using namespace scatha;
using namespace scatha::lex;
using namespace scatha::parse;
using namespace scatha::sem;


int main() {
	
	

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
		
		SemanticAnalyzer s;
		s.run(ast.get());
		
		printSymbolTable(s.symbolTable());
		
//		ast::printTree(ast.get());
		
//		ast::printSource(ast.get());
	}
	catch (std::exception const& e) {
		std::cout << e.what() << std::endl;
	}
	
}

