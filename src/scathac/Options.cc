#include "Options.h"

#include <cstdlib>
#include <optional>
#include <stdexcept>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>

using namespace scatha;
using namespace ranges::views;

static std::optional<std::filesystem::path> findStdlibDir(
    BaseOptions const& options) {
    if (!options.stdlibDir.empty()) return options.stdlibDir;
    if (char const* stdlibDir = std::getenv("SCATHA_STDLIB_DIR"))
        return std::filesystem::path(stdlibDir);
#if defined(SCATHA_DEFAULT_STDLIB_DIR)
    return std::filesystem::path(SCATHA_DEFAULT_STDLIB_DIR);
#else
    return std::nullopt;
#endif
}

void scatha::populateBaseOptions(BaseOptions const& options,
                                 CompilerInvocation& invocation) {
    invocation.addInputs(loadSourceFiles(options.files));
    invocation.addLibSearchPaths(options.libSearchPaths);
    if (auto dir = findStdlibDir(options)) invocation.addLibSearchPath(*dir);
    invocation.generateDebugInfo(options.generateDebugInfo);
}

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
