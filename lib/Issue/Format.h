#ifndef SCATHA_ISSUE_FORMAT_H_
#define SCATHA_ISSUE_FORMAT_H_

#include <cassert>
#include <iosfwd>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <utl/hashtable.hpp>

#include "Common/Base.h"
#include "Common/SourceFile.h"
#include "Common/SourceLocation.h"
#include "Issue/IssueSeverity.h"
#include "Issue/Message.h"
#include "Issue/SourceStructure.h"

namespace scatha {

/// Map that lazily creates `SourceStructure` object for source files as
/// necessary
class SourceStructureMap {
public:
    explicit SourceStructureMap(std::span<SourceFile const> files):
        files(files) {}

    ///
    SourceStructure const& operator()(size_t index) {
        /// TODO: Remove this.
        /// This is a preliminary hack because we still support calling the
        /// issue `print()` functions with a single string view for a source
        /// file
        if (files.size() == 1) {
            index = 0;
        }
        auto itr = map.find(index);
        if (itr == map.end()) {
            auto& file = files[index];
            itr =
                map.insert({ index, SourceStructure(file.path(), file.text()) })
                    .first;
        }
        return itr->second;
    }

private:
    utl::hashmap<size_t, SourceStructure> map;
    std::span<SourceFile const> files;
};

/// Legacy interface
void SCTEST_API highlightSource(SourceStructureMap& source,
                                SourceRange sourceRange,
                                IssueSeverity severity);

/// Legacy interface
void SCTEST_API highlightSource(SourceStructureMap& source,
                                SourceRange sourceRange, IssueSeverity severity,
                                std::ostream& ostream);

/// Prints the source highlights \p highlights to \p ostream
void SCTEST_API highlightSource(SourceStructureMap& source,
                                std::vector<SourceHighlight> highlights,
                                IssueSeverity severity, std::ostream& ostream);

} // namespace scatha

#endif // SCATHA_ISSUE_FORMAT_H_
