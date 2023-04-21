#ifndef ISSUE_FORMAT_H_
#define ISSUE_FORMAT_H_

#include <iosfwd>
#include <string_view>

#include <scatha/AST/Token.h>
#include <scatha/Common/Base.h>

namespace scatha::issue {

SCATHA_API void highlightToken(std::string_view text, Token const& token);

SCATHA_API void highlightToken(std::string_view text,
                               Token const& token,
                               std::ostream& ostream);

} // namespace scatha::issue

#endif // ISSUE_FORMAT_H_
