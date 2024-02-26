#ifndef FUZZ_LEX_H_
#define FUZZ_LEX_H_

#include <filesystem>
#include <string_view>

namespace scatha {

///
class LexerFuzzer {
public:
    LexerFuzzer() = default;

    /// Construct a fuzzer from a text file
    explicit LexerFuzzer(std::filesystem::path const& path);

    /// Load a test case from a file
    void loadFile(std::filesystem::path const& path);

    /// Repeatedly generate random text and run the lexer on it
    void run();

    /// Run the lexer on the current text
    void fuzzOne();

    /// Write the current text to a file
    void dumpCurrentToTestCase();

private:
    std::string text;
};

} // namespace scatha

#endif // FUZZ_LEX_H_
