#include "Parser/SourceFile.h"

#include <fstream>
#include <sstream>

#include <utl/strcat.hpp>

using namespace scatha;
using namespace parser;

SourceFile SourceFile::load(std::filesystem::path path) {
    std::fstream file(path);
    if (!file) {
        throw std::runtime_error(utl::strcat("Failed to open file ", path));
    }
    std::stringstream sstr;
    sstr << file.rdbuf();
    return make(std::move(sstr).str(), std::move(path));
}

SourceFile SourceFile::make(std::string text, std::filesystem::path path) {
    return SourceFile(std::move(path), std::move(text));
}

SourceFile::SourceFile(std::filesystem::path path, std::string text):
    _path(path), _text(text) {}
