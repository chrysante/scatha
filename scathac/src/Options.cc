#include "Options.h"

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <stdexcept>

using namespace scatha;
using namespace ranges::views;

FrontendType scatha::deduceFrontend(
    std::span<std::filesystem::path const> files) {
    if (files.empty()) {
        throw std::runtime_error("No input files");
    }
    auto hasExt = [](std::string ext) {
        return [=](auto& path) { return path.extension() == ext; };
    };
    if (ranges::all_of(files, hasExt(".sc"))) {
        return FrontendType::Scatha;
    }
    if (ranges::all_of(files, hasExt(".scir")) && files.size() == 1) {
        return FrontendType::IR;
    }
    if (files.size() <= 1) {
        throw std::runtime_error("Invalid file extension");
    }
    else {
        throw std::runtime_error("Invalid combination of file extensions");
    }
}

std::vector<SourceFile> scatha::loadSourceFiles(
    std::span<std::filesystem::path const> files) {
    return files | transform(&SourceFile::load) | ranges::to<std::vector>;
}
