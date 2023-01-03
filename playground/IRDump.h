#ifndef PLAYGROUND_IRDUMP_H_
#define PLAYGROUND_IRDUMP_H_

#include <filesystem>
#include <string>

namespace playground {

void irDump(std::string text);

void irDump(std::filesystem::path file);

} // namespace playground

#endif // PLAYGROUND_IRDUMP_H_

