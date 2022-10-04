#ifndef SCATHA_PARSER_PARSERERROR_H_
#define SCATHA_PARSER_PARSERERROR_H_

#include <stdexcept>
#include <string>

#include "Basic/Basic.h"
#include "Common/ProgramIssue.h"

namespace scatha::parse {
	
	class SCATHA(API) ParsingIssue: public ProgramIssue {
	public:
		explicit ParsingIssue(TokenEx const&, std::string const& message);
	};
	
	void expectIdentifier(TokenEx const&, std::string_view message = {});
	void expectKeyword(TokenEx const&, std::string_view message = {});
	void expectDeclarator(TokenEx const&, std::string_view message = {});
	void expectSeparator(TokenEx const&, std::string_view message = {});
	void expectID(TokenEx const&, std::string_view, std::string_view message = {});
	
}
	
#endif // SCATHA_PARSER_PARSERERROR_H_

