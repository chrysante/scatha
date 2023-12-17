#ifndef SCATHAC_OPTIONS_H_
#define SCATHAC_OPTIONS_H_

#include <filesystem>
#include <string>
#include <vector>

namespace scatha {

/// Common command line options
struct OptionsBase {
    /// List of all input files
    std::vector<std::filesystem::path> files;

    /// List of library search paths
    std::vector<std::filesystem::path> libSearchPaths;

    /// Optimization level
    int optLevel = -1;

    /// Custom IR optimization pipeline
    std::string pipeline;
};

/// Parsing mode
enum class ParseMode { Scatha, IR };

/// \Returns the mode based on the extension of the input files. Only used for
/// debug tools
ParseMode getMode(OptionsBase const& options);

} // namespace scatha

#endif // SCATHAC_OPTIONS_H_
