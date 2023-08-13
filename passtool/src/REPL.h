#ifndef SCATHA_PASSTOOL_REPL_H_
#define SCATHA_PASSTOOL_REPL_H_

#include <filesystem>
#include <vector>

#include <CLI/CLI11.hpp>

#include "Command.h"

namespace scatha::passtool {

class REPL: public Command {
public:
    explicit REPL(CLI::App* parent);

    int run();

private:
    std::vector<std::filesystem::path> paths;
    bool ir = false;
};

} // namespace scatha::passtool

#endif // SCATHA_PASSTOOL_REPL_H_
