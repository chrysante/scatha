#include "Util.h"

#include <random>
#include <stdexcept>

#include <scatha/Common/APInt.h>
#include <utl/strcat.hpp>

using namespace scatha;

std::fstream scatha::openFile(std::filesystem::path const& path,
                              std::ios::fmtflags flags) {
    std::fstream file(path, flags);
    if (!file) {
        throw std::runtime_error(utl::strcat("Failed to open ", path));
    }
    return file;
}

std::fstream scatha::makeTestCaseFile(std::string folderName) {
    std::filesystem::create_directory(folderName);
    auto filename = utl::strcat("testcase-",
                                APInt(std::random_device{}(), 64).toString(36),
                                ".txt");
    auto path = std::filesystem::path(folderName) / filename;
    return openFile(path, std::ios::trunc | std::ios::out);
}

std::string scatha::generateRandomString(size_t minSize, size_t maxSize) {
    std::mt19937_64 rng{ std::random_device{}() };
    size_t size = std::uniform_int_distribution<size_t>(100, 1000)(rng);
    std::string text;
    text.reserve(size);
    std::uniform_int_distribution<int> dist(32, 126);
    for (size_t i = 0; i < size; ++i) {
        text.push_back(static_cast<char>(dist(rng)));
    }
    return text;
}
