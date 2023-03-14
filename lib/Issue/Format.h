#ifndef ISSUE_FORMAT_H_
#define ISSUE_FORMAT_H_

#include <iosfwd>

#include "Basic/Basic.h"
#include "Common/StructuredSource.h"
#include "Common/Token.h"

namespace scatha::issue {

SCATHA(API)
void highlightToken(StructuredSource const& source, Token const& token);

SCATHA(API)
void highlightToken(StructuredSource const& source,
                    Token const& token,
                    std::ostream& ostream);

} // namespace scatha::issue

#endif // ISSUE_FORMAT_H_
