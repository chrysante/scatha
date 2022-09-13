#include <iostream>
#include <string>

#include "Lexer/Lexer.h"
#include "Parser/Parser.h"

using namespace scatha;
using namespace scatha::parse;


int main() {
	
	true ? 1 : 2, 3;
	
	std::string const text = R"(

fn mul(a: int, b: int) -> int {

	a = b ? c : d;


	
}

fn main() -> void {
	a[a, b, c, d]
}

)";
	try {
		lex::Lexer l(text);
		auto tokens = l.lex();
		
		std::cout << "--- Tokens: \n";
		for (auto& t: tokens) {
			std::cout << t << std::endl;
		}
		
		std::cout << "\n\n\n--- AST: \n";
		Parser p(tokens);
		auto ast = p.parse();
		
		std::cout << *ast << std::endl;
		
		
		
	}
	catch (std::exception const& e) {
		std::cout << e.what() << std::endl;
	}
	
}
