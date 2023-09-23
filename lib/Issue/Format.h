#ifndef SCATHA_ISSUE_FORMAT_H_
#define SCATHA_ISSUE_FORMAT_H_

#include <cassert>
#include <iosfwd>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Common/Base.h"
#include "Common/SourceLocation.h"
#include "Issue/SourceStructure.h"

namespace scatha {

///
void SCTEST_API highlightSource(SourceStructure const& source,
                                SourceRange sourceRange);

///
void SCTEST_API highlightSource(SourceStructure const& source,
                                SourceRange sourceRange,
                                std::ostream& ostream);

///
struct SourceHighlight {
    SourceRange position;
    std::string message;
};

///
void SCTEST_API highlightSource(SourceStructure const& source,
                                std::vector<SourceHighlight>,
                                std::ostream& ostream);

} // namespace scatha

#endif // SCATHA_ISSUE_FORMAT_H_
