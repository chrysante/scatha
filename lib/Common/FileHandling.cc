#include "Common/FileHandling.h"

#include <fstream>
#include <stdexcept>

#include <utl/strcat.hpp>

using namespace scatha;

std::fstream scatha::createOutputFile(std::filesystem::path const& path,
                                      std::ios::openmode flags) {

    if (auto parent = path.parent_path();
        !parent.empty() && !std::filesystem::exists(parent))
    {
        std::filesystem::create_directories(parent);
    }
    flags |= std::ios::out;
    std::fstream file(path, flags);
    if (!file) {
        throw std::runtime_error(utl::strcat("Failed to create ", path));
    }
    return file;
}
