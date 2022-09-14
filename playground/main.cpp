#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include <sstream>

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
		
//		std::cout << "--- Tokens: \n";
//		for (auto& t: tokens) {
//			std::cout << t << std::endl;
//		}
//		std::cout << "\n\n\n--- AST: \n";
		
		Parser p(tokens);
		auto ast = p.parse();
		
		std::cout << *ast << std::endl;
	}
	catch (std::exception const& e) {
		std::cout << e.what() << std::endl;
	}
	
}
