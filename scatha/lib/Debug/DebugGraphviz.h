#ifndef SCATHA_DEBUG_DEBUGGRAPHVIZ_H_
#define SCATHA_DEBUG_DEBUGGRAPHVIZ_H_

#include <filesystem>
#include <fstream>
#include <string>

/// This file defines debug utilities that can be used to generate graphs using
/// graphviz on the fly using the debugger

namespace scatha::debug {

/// Creates a new file in a temporary directory within the working directory
std::pair<std::filesystem::path, std::fstream> newDebugFile();

/// Creates a new temporary file with name \p name
std::pair<std::filesystem::path, std::fstream> newDebugFile(std::string name);

/// Reads the file at \p filepath as graphviz source, generates a `.svg` file of
/// the code in the same directory and issues a system call to open the
/// generated file
void createGraphAndOpen(std::filesystem::path filepath);

} // namespace scatha::debug

#endif // SCATHA_DEBUG_DEBUGGRAPHVIZ_H_
