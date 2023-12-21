#include "Lex.h"

#include <random>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include <APMath/APInt.h>
#include <utl/strcat.hpp>
#include <scatha/Parser/Lexer.h>
#include <scatha/Issue/IssueHandler.h>

using namespace scatha;

static std::fstream openFile(std::filesystem::path const& path, std::ios::fmtflags flags = std::ios::in | std::ios::out) {
    std::fstream file(path, flags);
    if (!file) {
        throw std::runtime_error(utl::strcat("Failed to open ", path));
    }
    return file;
}

static std::string generateInput() {
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

LexerFuzzer::LexerFuzzer(std::filesystem::path const& path) {
    loadFile(path);
}

void LexerFuzzer::loadFile(std::filesystem::path const& path) {
    auto file = openFile(path);
    std::stringstream sstr;
    sstr << file.rdbuf();
    text = std::move(sstr).str();
}

void LexerFuzzer::run() {
    while (true) {
        text = generateInput();
        fuzzOne();
    }
}

void LexerFuzzer::fuzzOne() {
    IssueHandler iss;
    parser::lex(text, iss);
}

void LexerFuzzer::dumpCurrentToTestCase() {
    std::filesystem::create_directory("lex");
    auto filename = utl::strcat("testcase-", APInt(std::random_device{}(), 64).toString(36), ".txt");
    auto path = std::filesystem::path("lex") / filename;
    auto file = openFile(path, std::ios::trunc | std::ios::out);
    file << text;
}
