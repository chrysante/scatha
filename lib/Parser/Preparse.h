#ifndef PARSER_PREPARSER_H_
#define PARSER_PREPARSER_H_

#include <utl/vector.hpp>

#include "Basic/Basic.h"
#include "Common/Token.h"
#include "Issue/IssueHandler.h"

namespace scatha::parse {
	
SCATHA(API) void preparse(utl::vector<Token>& tokens, issue::SyntaxIssueHandler& issueHandler);
	
}

#endif // PARSER_PREPARSER_H_

