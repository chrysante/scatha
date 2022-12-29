#ifndef PLAYGROUND_SAMPLECOMPILER_H_
#define PLAYGROUND_SAMPLECOMPILER_H_

#include <filesystem>
#include <string>

namespace playground {

void compile(std::string text);

void compile(std::filesystem::path file);

} // namespace playground

#endif // PLAYGROUND_SAMPLECOMPILER_H_

