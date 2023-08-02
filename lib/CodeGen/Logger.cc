#include "CodeGen/Logger.h"

#include <iostream>

#include "Common/Logging.h"
#include "MIR/Print.h"

using namespace scatha::cg;

DebugLogger::DebugLogger(): ostream(std::cout) {}

void DebugLogger::log(std::string_view stage, mir::Module const& mod) {
    logging::header(stage, ostream);
    mir::print(mod, ostream);
}
