#ifndef SCATHA_ISSUE_FORMAT_H_
#define SCATHA_ISSUE_FORMAT_H_

#include <iosfwd>
#include <string_view>

#include "Common/Base.h"
#include "Common/SourceLocation.h"

namespace scatha {

void highlightSource(std::string_view text, SourceRange sourceRange);

void highlightSource(std::string_view text,
                     SourceRange sourceRange,
                     std::ostream& ostream);

} // namespace scatha

#endif // SCATHA_ISSUE_FORMAT_H_
