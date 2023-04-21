#ifndef ISSUE_FORMAT_H_
#define ISSUE_FORMAT_H_

#include <iosfwd>
#include <string_view>

#include <scatha/Common/Base.h>
#include <scatha/Common/Token.h>

namespace scatha::issue {

SCATHA_API void highlightToken(std::string_view text, Token const& token);

SCATHA_API void highlightToken(std::string_view text,
                               Token const& token,
                               std::ostream& ostream);

} // namespace scatha::issue

#endif // ISSUE_FORMAT_H_
