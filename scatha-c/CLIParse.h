#ifndef SCATHAC_CLIPARSE_H_
#define SCATHAC_CLIPARSE_H_

#include <filesystem>

namespace scathac {

struct Options {
    std::filesystem::path filepath;
    bool run;
    bool time;
};

Options parseCLI(int argc, char* argv[]);

} // namespace scathac

#endif // SCATHAC_CLIPARSE_H_
