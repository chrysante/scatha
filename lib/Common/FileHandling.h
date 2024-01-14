#ifndef SCATHA_COMMON_FILEHANDLING_H_
#define SCATHA_COMMON_FILEHANDLING_H_

#include <filesystem>
#include <ios>

namespace scatha {

/// Creates a new file or overrides an the existing file at \p dest
/// Non-existent parent directories are created if necessary
/// \Throws an exception of type `std::runtime_error` if the file could not be
/// created
std::fstream createOutputFile(std::filesystem::path const& dest,
                              std::ios::openmode flags);

} // namespace scatha

#endif // SCATHA_COMMON_FILEHANDLING_H_
