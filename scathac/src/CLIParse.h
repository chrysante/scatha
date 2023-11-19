#ifndef SCATHAC_CLIPARSE_H_
#define SCATHAC_CLIPARSE_H_

#include <filesystem>
#include <vector>

namespace scathac {

struct Options {
    /// List of all input files
    std::vector<std::filesystem::path> files;

    /// Output directory
    std::filesystem::path bindir;

    /// Set if program shall be optimized
    bool optimize;

    /// Set if time taken by compilation shall be printed
    bool time;

    ///
    bool binaryOnly;

    /// Set if debug symbols shall be generated
    bool debug;
};

Options parseCLI(int argc, char* argv[]);

} // namespace scathac

#endif // SCATHAC_CLIPARSE_H_
