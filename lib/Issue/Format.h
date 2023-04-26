#ifndef SCATHA_ISSUE_FORMAT_H_
#define SCATHA_ISSUE_FORMAT_H_

#include <iosfwd>
#include <string_view>

#include <scatha/AST/SourceLocation.h>
#include <scatha/Common/Base.h>

namespace scatha {

SCATHA_API void highlightSource(std::string_view text, SourceRange sourceRange);

SCATHA_API void highlightSource(std::string_view text,
                                SourceRange sourceRange,
                                std::ostream& ostream);

} // namespace scatha

#endif // SCATHA_ISSUE_FORMAT_H_
