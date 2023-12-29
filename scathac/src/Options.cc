#include "Options.h"

#include <range/v3/algorithm.hpp>
#include <stdexcept>

using namespace scatha;

ParseMode scatha::getMode(OptionsBase const& options) {
    if (options.files.empty()) {
        throw std::runtime_error("No input files");
    }
    auto hasExt = [](std::string ext) {
        return [=](auto& path) { return path.extension() == ext; };
    };
    if (ranges::all_of(options.files, hasExt(".sc"))) {
        return ParseMode::Scatha;
    }
    if (ranges::all_of(options.files, hasExt(".scir")) &&
        options.files.size() == 1)
    {
        return ParseMode::IR;
    }
    if (options.files.size() <= 1) {
        throw std::runtime_error("Invalid file extension");
    }
    else {
        throw std::runtime_error("Invalid combination of file extensions");
    }
}
