#ifndef FUZZ_UTIL_H_
#define FUZZ_UTIL_H_

#include <filesystem>
#include <fstream>
#include <string>

namespace scatha {

///
std::fstream openFile(std::filesystem::path const& path,
                      std::ios::fmtflags flags = std::ios::in | std::ios::out);

///
std::fstream makeTestCaseFile(std::string folderName);

///
std::string generateRandomString(size_t minSize, size_t maxSize);

} // namespace scatha

#endif // FUZZ_UTIL_H_
