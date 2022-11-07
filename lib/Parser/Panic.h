#ifndef SCATHA_PARSE_PANIC_H_
#define SCATHA_PARSE_PANIC_H_

#include <span>
#include <string_view>

namespace scatha::parse {
	
struct PanicOptions {
    std::string_view targetDelimiter = ";";
    bool eatDelimiter = true;
};

void panic(class TokenStream&, PanicOptions = {});
	
}

#endif // SCATHA_PARSE_PANIC_H_

