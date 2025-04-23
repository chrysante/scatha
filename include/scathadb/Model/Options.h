#ifndef SDB_MODEL_OPTIONS_H_
#define SDB_MODEL_OPTIONS_H_

#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace sdb {

///
struct Options {
    std::filesystem::path filepath;
    std::vector<std::string> arguments;

    explicit operator bool() const {
        return !filepath.empty() || !arguments.empty();
    }
};

///
Options parseArguments(std::span<std::string const> arguments);

} // namespace sdb

#endif // SDB_MODEL_OPTIONS_H_
