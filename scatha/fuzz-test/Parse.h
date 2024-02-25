#ifndef FUZZ_PARSE_H_
#define FUZZ_PARSE_H_

#include <filesystem>
#include <string_view>

#include "Parser/TokenStream.h"

namespace scatha {

///
class ParserFuzzer {
public:
    ParserFuzzer() = default;

    /// Construct a fuzzer from a text file
    explicit ParserFuzzer(std::filesystem::path const& path);

    /// Load a test case from a file
    void loadFile(std::filesystem::path const& path);

    /// Repeatedly generate random token stream and run the parser on it
    void runRandom();

    /// Repeatedly swap a few tokens in the current text and parse the resulting
    /// token stream
    void runModify();

    /// Run the parser on the current token stream
    void fuzzOne();

    /// Write the current text to a file
    void dumpCurrentToTestCase();

private:
    std::string text;
};

} // namespace scatha

#endif // FUZZ_PARSE_H_
