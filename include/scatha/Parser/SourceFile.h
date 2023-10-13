#ifndef SCATHA_PARSER_SOURCEFILE_H_
#define SCATHA_PARSER_SOURCEFILE_H_

#include <filesystem>
#include <string>

#include <scatha/Common/Base.h>

namespace scatha::parser {

/// Represents a source file that is potentially loaded into memory
class SCATHA_API SourceFile {
public:
    /// Construct a source file from a file path. Loads the file into memory
    static SourceFile load(std::filesystem::path path);

    /// Construct a source file from a string
    static SourceFile make(std::string text, std::filesystem::path path = {});

    /// \Returns the file content
    std::string const& text() const { return _text; }

    /// \returns the filepath
    std::filesystem::path const& path() const { return _path; }

private:
    SourceFile(std::filesystem::path, std::string text);

    std::filesystem::path _path;
    std::string _text;
};

} // namespace scatha::parser

#endif // SCATHA_PARSER_SOURCEFILE_H_
