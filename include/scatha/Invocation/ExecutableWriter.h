#ifndef SCATHA_INVOCATION_EXECUTABLEWRITER_H_
#define SCATHA_INVOCATION_EXECUTABLEWRITER_H_

#include <filesystem>
#include <span>

#include <scatha/Common/Base.h>

namespace scatha {

/// Options struct for `writeExecutableFile()`
struct ExecWriteOptions {
    /// Prepend a bash script to the file and set the executable file flag to be
    /// make it executable
    bool executable = true;
};

/// Writes compiled programs to disk.
/// \param dest The output filename
/// \param program The compiled binary
/// \param options Set of otions control file emission
SCATHA_API void writeExecutableFile(std::filesystem::path dest,
                                    std::span<uint8_t const> program,
                                    ExecWriteOptions options = {});

} // namespace scatha

#endif // SCATHA_INVOCATION_EXECUTABLEWRITER_H_
