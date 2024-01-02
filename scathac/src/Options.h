#ifndef SCATHAC_OPTIONS_H_
#define SCATHAC_OPTIONS_H_

#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include <scatha/Common/SourceFile.h>
#include <scatha/Invocation/CompilerInvocation.h>

namespace scatha {

/// Common command line options
struct OptionsBase {
    /// List of all input files
    std::vector<std::filesystem::path> files;

    /// List of library search paths
    std::vector<std::filesystem::path> libSearchPaths;

    /// Output file stem
    std::filesystem::path outputFile;

    /// Optimization level
    int optLevel = 0;

    /// Custom IR optimization pipeline
    std::string pipeline;

    ///
    TargetType targetType = TargetType::Executable;
};

/// Parsing mode
enum class ParseMode { Scatha, IR };

/// \Returns the mode based on the extension of the input files. Only used for
/// debug tools
FrontendType deduceFrontend(std::span<std::filesystem::path const> files);

///
std::vector<SourceFile> loadSourceFiles(
    std::span<std::filesystem::path const> files);

} // namespace scatha

#endif // SCATHAC_OPTIONS_H_
