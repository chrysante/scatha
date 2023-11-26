#include "Common/ExecutableWriter.h"

#include <fstream>
#include <stdexcept>

#include <utl/strcat.hpp>

using namespace scatha;

/// Throws an error message to the console and exits the program
[[noreturn]] static void throwFileError(std::filesystem::path path) {
    throw std::runtime_error(utl::strcat("Failed to write file ", path));
}

/// Helper to write escaped bash command to a file. See documentation of
/// `writeBashHeader()`
static auto bashCommandEmitter(std::ostream& file) {
    return [&, i = 0](std::string_view command) mutable {
        if (i++ == 0) {
            file << "#!/bin/sh\n";
        }
        else {
            file << "#Shell command\n";
        }
        file << command << "\n";
    };
}

/// To emit files that are directly executable, we prepend a bash script to the
/// emitted binary file. That bash script executes the virtual machine with the
/// same file and exits. The convention for bash commands is one commented line
/// (starting with `#` and ending with `\n`) and one line of script (ending with
/// `\n`). This way the virtual machine identifies the bash commands and ignores
/// them
static void writeBashHeader(std::ostream& file) {
    auto emitter = bashCommandEmitter(file);
    emitter("svm --binary \"$0\" \"$@\"");
    emitter("exit $?");
}

/// Calls the system command `chmod` to permit execution of the specified file
static void permitExecution(std::filesystem::path filename) {
    std::system(utl::strcat("chmod +x ", filename.string()).data());
}

/// Revokes permission to execute the specified file
static void prohibitExecution(std::filesystem::path filename) {
    std::system(utl::strcat("chmod -x ", filename.string()).data());
}

/// Copies the program \p program to the file \p file
static void writeBinary(std::ostream& file, std::span<uint8_t const> program) {
    std::copy(program.begin(),
              program.end(),
              std::ostream_iterator<char>(file));
}

void scatha::writeExecutableFile(std::filesystem::path dest,
                                 std::span<uint8_t const> program,
                                 ExecWriteOptions options) {
    if (options.executable) {
        std::fstream file(dest, std::ios::out | std::ios::trunc);
        if (!file) {
            throwFileError(dest);
        }
        writeBashHeader(file);
        file.close();
        permitExecution(dest);
    }
    /// We open the file again, this time in binary mode, to ensure that no
    /// unwanted character conversions occur
    auto const flags = std::ios::out | std::ios::binary |
                       (options.executable ? std::ios::app : std::ios::trunc);
    std::fstream file(dest, flags);
    if (!file) {
        throwFileError(dest);
    }
    file.seekg(0, std::ios::end);
    writeBinary(file, program);
    file.close();
    /// If we don't generate an executable, we explicitly revoke permission to
    /// execute the file, because the same file could have been made executable
    /// by a previous invocation of the compiler
    if (!options.executable) {
        prohibitExecution(dest);
    }
}
