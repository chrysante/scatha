#include "Lex.h"

#include <fstream>
#include <random>
#include <sstream>
#include <stdexcept>

#include <scatha/Issue/IssueHandler.h>
#include <scatha/Parser/Lexer.h>

#include "Util.h"

using namespace scatha;

LexerFuzzer::LexerFuzzer(std::filesystem::path const& path) { loadFile(path); }

void LexerFuzzer::loadFile(std::filesystem::path const& path) {
    auto file = openFile(path);
    std::stringstream sstr;
    sstr << file.rdbuf();
    text = std::move(sstr).str();
}

void LexerFuzzer::run() {
    while (true) {
        text = generateRandomString(100, 1000);
        fuzzOne();
    }
}

void LexerFuzzer::fuzzOne() {
    IssueHandler iss;
    parser::lex(text, iss);
}

void LexerFuzzer::dumpCurrentToTestCase() {
    auto file = makeTestCaseFile("lex");
    file << text;
}
