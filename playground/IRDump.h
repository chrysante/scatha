#ifndef PLAYGROUND_IRDUMP_H_
#define PLAYGROUND_IRDUMP_H_

#include <filesystem>
#include <string>

namespace scatha::ir {

class Context;
class Module;

} // namespace scatha::ir

namespace playground {

void irDump(std::string text);

void irDump(std::filesystem::path file);

std::pair<scatha::ir::Context, scatha::ir::Module> makeIRModule(std::string text);

std::pair<scatha::ir::Context, scatha::ir::Module> makeIRModule(std::filesystem::path file);

} // namespace playground

#endif // PLAYGROUND_IRDUMP_H_
