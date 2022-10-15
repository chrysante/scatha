#ifndef SCATHA_PARSER_PARSERERROR_H_
#define SCATHA_PARSER_PARSERERROR_H_

#include <stdexcept>
#include <string>

#include "Basic/Basic.h"
#include "Common/ProgramIssue.h"

namespace scatha::parse {

class SCATHA(API) ParsingIssue: public ProgramIssue {
public:
    explicit ParsingIssue(Token const&, std::string const& message);
};

void expectIdentifier(Token const&, std::string_view message = {});
void expectKeyword(Token const&, std::string_view message = {});
void expectDeclarator(Token const&, std::string_view message = {});
void expectSeparator(Token const&, std::string_view message = {});
void expectID(Token const&, std::string_view, std::string_view message = {});

} // namespace scatha::parse

#endif // SCATHA_PARSER_PARSERERROR_H_
