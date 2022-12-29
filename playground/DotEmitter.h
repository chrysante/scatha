#ifndef PLAYGROUND_DOTEMITTER_H_
#define PLAYGROUND_DOTEMITTER_H_

#include <filesystem>
#include <string>

namespace scatha::ir {

class Module;

} // namespace scatha::ir

namespace playground {
	
std::string emitDot(scatha::ir::Module const& mod);

void emitDot(scatha::ir::Module const& mod, std::filesystem::path const& outFilepath);
	
} // namespace 

#endif // PLAYGROUND_DOTEMITTER_H_

