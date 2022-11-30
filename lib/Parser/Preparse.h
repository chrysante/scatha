#ifndef PARSER_PREPARSER_H_
#define PARSER_PREPARSER_H_

#include <utl/vector.hpp>

#include "Basic/Basic.h"
#include "Common/Token.h"

namespace scatha::parse {
	
void preparse(utl::vector<Token>& tokens);
	
}

#endif // PARSER_PREPARSER_H_

