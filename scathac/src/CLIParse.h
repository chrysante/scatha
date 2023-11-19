#ifndef SCATHAC_CLIPARSE_H_
#define SCATHAC_CLIPARSE_H_

#include <filesystem>
#include <vector>

namespace scathac {

struct Options {
    std::vector<std::filesystem::path> files;
    std::filesystem::path bindir;
    bool optimize;
    bool time;
    bool binaryOnly;
};

Options parseCLI(int argc, char* argv[]);

} // namespace scathac

#endif // SCATHAC_CLIPARSE_H_
