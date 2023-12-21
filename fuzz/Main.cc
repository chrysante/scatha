#include <iostream>

#include <CLI/CLI11.hpp>

#include "Lex.h"

using namespace scatha;

namespace {

struct LexOptions {
    std::filesystem::path file;
};

} // namespace

int main(int argc, const char *const *argv) {
    CLI::App fuzzer;
    fuzzer.require_subcommand(1, 1);
    auto* lexer = fuzzer.add_subcommand("lex");
    LexOptions lexOptions;
    lexer->add_option("-f,--file", lexOptions.file)->check(CLI::ExistingFile);
    
    try {
        fuzzer.parse(argc, argv);
        if (lexer->parsed()) {
            LexerFuzzer fuzzer;
            if (!lexOptions.file.empty()) {
                fuzzer.loadFile(lexOptions.file);
                fuzzer.fuzzOne();
                return 0;
            }
            fuzzer.run();
        }
    }
    catch (CLI::ParseError const& e) {
        return fuzzer.exit(e);
    }
    catch (std::exception const& e) {
        std::cout << e.what() << std::endl;
        return 1;
    }
}
