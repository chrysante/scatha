#include <iostream>

#include <CLI/CLI11.hpp>

#include "Lex.h"
#include "Parse.h"

using namespace scatha;

namespace {

struct OptionsBase {
    std::filesystem::path file;
};

struct LexOptions: OptionsBase {};

struct ParseOptions: OptionsBase {
    std::filesystem::path modFile;
};

} // namespace

int main(int argc, char const* const* argv) {
    CLI::App fuzzer;
    fuzzer.require_subcommand(1, 1);

    auto* lexer = fuzzer.add_subcommand("lex");
    LexOptions lexOptions;
    lexer->add_option("-f,--file", lexOptions.file)->check(CLI::ExistingFile);

    auto* parser = fuzzer.add_subcommand("parse");
    ParseOptions parseOptions;
    parser->add_option("-f,--file", parseOptions.file)
        ->check(CLI::ExistingFile);
    parser->add_option("-m,--modify", parseOptions.modFile)
        ->check(CLI::ExistingFile);

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
        if (parser->parsed()) {
            ParserFuzzer fuzzer;
            if (!parseOptions.file.empty()) {
                fuzzer.loadFile(parseOptions.file);
                fuzzer.fuzzOne();
                return 0;
            }
            if (!parseOptions.modFile.empty()) {
                fuzzer.loadFile(parseOptions.modFile);
                fuzzer.runModify();
                return 0;
            }
            fuzzer.runRandom();
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
