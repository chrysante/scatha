#ifndef SCATHA_COMMON_DEBUGINFO_H_
#define SCATHA_COMMON_DEBUGINFO_H_

#include <filesystem>
#include <span>
#include <string>
#include <variant>
#include <vector>

#include <scatha/Common/SourceLocation.h>

namespace scatha::dbi {

using SourceFileList = std::vector<std::filesystem::path>;

/// Converts debug info into a JSON string
std::string serialize(SourceFileList const* sourceFiles,
                      std::span<SourceLocation const* const> sourceLocations);

} // namespace scatha::dbi

#endif // SCATHA_COMMON_DEBUGINFO_H_
