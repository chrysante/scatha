#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include <sstream>

#include "AST/PrintSource.h"
#include "AST/PrintTree.h"
#include "AST/TypeChecker.h"
#include "Lexer/Lexer.h"
#include "Parser/Parser.h"

using namespace scatha;
using namespace scatha::parse;

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
		lex::Lexer l(text);
		auto tokens = l.lex();
		
		Parser p(tokens);
		auto ast = p.parse();
		
		ast::TypeChecker typechecker;
		typechecker.run(ast.get());
		
		ast::printTree(ast.get());
		
//		ast::printSource(ast.get());
	}
	catch (std::exception const& e) {
		std::cout << e.what() << std::endl;
	}
	
}

