#ifndef SCATHA_COMMON_FILEHANDLING_H_
#define SCATHA_COMMON_FILEHANDLING_H_

#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace scatha {

/// Creates a new file or overrides an the existing file at \p dest
/// Non-existent parent directories are created if necessary
/// \Throws an exception of type `std::runtime_error` if the file could not be
/// created
std::fstream createOutputFile(std::filesystem::path const& dest,
                              std::ios::openmode flags);

/// Represents a "tar" file containing multiple other files
class Archive {
public:
    ///
    enum class Mode { Read, Write, Closed };

    /// # Static constructors

    /// Opens "tar" archive at \p path for reading
    /// \Throws `std::runtime_error` if the file \p name does not exist
    static Archive Open(std::filesystem::path path);

    /// Create a "tar" archive at \p path for writing
    static Archive Create(std::filesystem::path path);

    /// # Queries

    /// \Returns the mode with which this archive was created
    Mode mode() const { return _mode; }

    /// \Returns a view over the files in this archive
    std::span<std::string const> files() const { return _files; }

    /// # Modifiers

    /// Closes the archive
    void close();

    ///
    ~Archive() { close(); }

    /// # Reading

    /// \Returns the contents of the file \p name as a string
    /// \Pre `mode()` must be `Read`
    std::optional<std::string> openTextFile(std::string const& name);

    /// \Returns the contents of the file \p name as binary data
    /// \Pre `mode()` must be `Read`
    std::optional<std::vector<unsigned char>> openBinaryFile(
        std::string const& name);

    /// # Writing

    ///
    /// \Pre `mode()` must be `Write`
    void addTextFile(std::string const& name, std::string_view contents);

    ///
    /// \Pre `mode()` must be `Write`
    void addBinaryFile(std::string const& name,
                       std::span<unsigned char const> contents);

    struct Impl {
        alignas(8) char data[64];
    };

private:
    explicit Archive(Mode mode, std::vector<std::string> files, Impl impl);

    template <typename T>
    std::optional<T> readImpl(std::string const& name);

    void writeImpl(std::string const& name, void const* data, size_t size);

    Mode _mode;
    std::vector<std::string> _files;
    Impl impl;
};

} // namespace scatha

#endif // SCATHA_COMMON_FILEHANDLING_H_
