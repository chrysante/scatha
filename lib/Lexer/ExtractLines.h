#ifndef LEXER_EXTRACTLINES_H_
#define LEXER_EXTRACTLINES_H_

#include <string>
#include <string_view>

#include <utl/vector.hpp>

#include "Basic/Basic.h"

namespace scatha::lex {

SCATHA_API utl::vector<std::string> extractLines(std::string_view text);

}

#endif // LEXER_EXTRACTLINES_H_
