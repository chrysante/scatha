#include <Catch/Catch2.hpp>

#include <iostream>

#include "Lexer/Lexer.h"
#include "Parser/Parser.h"

using namespace scatha;
using namespace parse;

TEST_CASE() {
	std::string const text = R"(
fn

 mul(a:int,b:int)->


int{
			var result = a
return

result

}

fn mul(a:int,b:int)->int{
	let result:int=a;
	let somethingElse: string="Hello World";
	return result
}

)";
	try {
	
		lex::Lexer l(text);
		
		auto tokens = l.lex();
		
		
		
		Allocator alloc;
		Parser p(tokens, alloc);

		auto* parseTree = p.parse();
	
		
		std::cout << *parseTree << std::endl;
		
	}
	catch (std::exception const& e) {
		std::cout << e.what() << std::endl;
	}
	
}
