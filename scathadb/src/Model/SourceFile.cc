#include "Model/SourceFile.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

#include <range/v3/view.hpp>
#include <utl/strcat.hpp>
#include <utl/utility.hpp>

using namespace sdb;

static std::vector<std::string_view> splitLines(std::string_view text) {
    return text | ranges::views::split('\n') |
           ranges::views::transform([](auto const& rng) {
        auto size = utl::narrow_cast<size_t>(ranges::distance(rng));
        return std::string_view(&*rng.begin(), size);
    }) | ranges::to<std::vector>;
}

SourceFile::SourceFile(std::filesystem::path path, std::string text):
    _path(std::move(path)), _text(std::move(text)), _lines(splitLines(_text)) {}

SourceFile SourceFile::Load(std::filesystem::path path) {
    std::fstream file(path);
    if (!file) {
        throw std::runtime_error(utl::strcat("Failed to load file ", path));
    }
    std::stringstream sstr;
    sstr << file.rdbuf();
    return SourceFile(path, std::move(sstr).str());
}
