#ifndef PLAYGROUND_SAMPLECOMPILER_H_
#define PLAYGROUND_SAMPLECOMPILER_H_

#include <filesystem>
#include <string>

namespace scatha::ir {

class Module;

} // namespace scatha::ir

namespace playground {

void compile(std::string text);

void compile(std::filesystem::path file);

} // namespace playground

#endif // PLAYGROUND_SAMPLECOMPILER_H_
