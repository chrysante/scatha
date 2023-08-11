#ifndef SCATHA_PASSTOOL_OPT_H_
#define SCATHA_PASSTOOL_OPT_H_

#include <filesystem>
#include <vector>

#include <CLI/CLI11.hpp>

#include "Command.h"

namespace scatha::passtool {

class Opt: public Command {
public:
    explicit Opt(CLI::App* parent);

    int run();

private:
    struct Options {};

    Options options;

    std::vector<std::filesystem::path> paths;
    std::filesystem::path outdir;
};

} // namespace scatha::passtool

#endif // SCATHA_PASSTOOL_OPT_H_
