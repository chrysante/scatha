#include "Common/SourceFile.h"

#include <fstream>
#include <sstream>

#include <utl/strcat.hpp>

using namespace scatha;

SourceFile SourceFile::load(std::filesystem::path path) {
    std::fstream file(path);
    if (!file) {
        throw std::runtime_error(utl::strcat("Failed to open file ", path));
    }
    std::stringstream sstr;
    sstr << file.rdbuf();
    return make(std::move(sstr).str(), std::filesystem::absolute(path));
}

SourceFile SourceFile::make(std::string text, std::filesystem::path path) {
    return SourceFile(std::move(path), std::move(text));
}

SourceFile::SourceFile(std::filesystem::path path, std::string text):
    _path(path), _text(text) {}
