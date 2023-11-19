#ifndef SDB_MODEL_SOURCEFILE_H_
#define SDB_MODEL_SOURCEFILE_H_

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sdb {

/// View over the lines of a source file
class SourceFile {
public:
    SourceFile() = default;

    /// Construct a source file from raw text
    explicit SourceFile(std::filesystem::path path, std::string text);

    /// Load a source file from a file path
    static SourceFile Load(std::filesystem::path path);

    /// \Returns the path where this source file is located
    std::filesystem::path const& path() const { return _path; }

    /// \Returns the raw text of this source file
    std::string_view text() const { return _text; }

    /// \Returns a list of string views of the lines of this source file
    std::span<std::string_view const> lines() const { return _lines; }

    /// \Returns the line at index \p index
    std::string_view line(size_t index) const { return _lines[index]; }

    /// \Returns the number of lines in this source file
    size_t numLines() const { return _lines.size(); }

private:
    std::filesystem::path _path;
    std::string _text;
    std::vector<std::string_view> _lines;
};

} // namespace sdb

#endif // SDB_MODEL_SOURCEFILE_H_
