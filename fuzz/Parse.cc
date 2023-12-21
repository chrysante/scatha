#include "Parse.h"

#include <iostream>
#include <sstream>

#include <scatha/Parser/Lexer.h>
#include <scatha/Parser/Parser.h>

#include "Util.h"

using namespace scatha;

ParserFuzzer::ParserFuzzer(std::filesystem::path const& path) {
    loadFile(path);
}

void ParserFuzzer::loadFile(std::filesystem::path const& path) {
    auto file = openFile(path);
    std::stringstream sstr;
    sstr << file.rdbuf();
    text = std::move(sstr).str();
}

void ParserFuzzer::runRandom() {
    for (int i = 0;; ++i) {
        text = generateRandomString(100, 1000);
        fuzzOne();
        std::cout << i << std::endl;
    }
}

void ParserFuzzer::runModify() {
    IssueHandler iss;
    auto baseTokens = parser::lex(text, iss);
    assert(iss.empty());
    for (int i = 0;; ++i) {
        auto tokens = baseTokens;
        std::mt19937_64 rng{ std::random_device{}() };
        size_t numSwaps = std::uniform_int_distribution<size_t>(1, 4)(rng);
        std::uniform_int_distribution<size_t> dist(0, tokens.size() - 1);
        for (size_t i = 0; i < numSwaps; ++i) {
            std::swap(tokens[dist(rng)], tokens[dist(rng)]);
        }
        std::stringstream sstr;
        for (auto& token: tokens) {
            sstr << token.id() << " ";
        }
        text = std::move(sstr).str();
        fuzzOne();
        std::cout << i << std::endl;
    }
}

void ParserFuzzer::fuzzOne() {
    IssueHandler iss;
    parser::parse(text, iss);
}

void ParserFuzzer::dumpCurrentToTestCase() {
    auto file = makeTestCaseFile("parse");
    file << text;
}
