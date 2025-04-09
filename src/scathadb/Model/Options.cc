#include "Model/Options.h"

#include <range/v3/view.hpp>

using namespace sdb;

Options sdb::parseArguments(std::span<std::string const> args) {
    if (args.empty()) {
        return {};
    }
    Options options{};
    options.filepath = args.front();
    if (std::filesystem::exists(options.filepath)) {
        options.filepath = std::filesystem::absolute(options.filepath);
    }
    for (auto& arg: args | ranges::views::drop(1)) {
        options.arguments.push_back(arg);
    }
    return options;
}
