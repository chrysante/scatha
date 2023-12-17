#ifndef SCATHA_IRGEN_FFILINKER_H_
#define SCATHA_IRGEN_FFILINKER_H_

#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include "IR/Fwd.h"
#include "IRGen/IRGen.h"

namespace scatha::irgen {

///
Expected<void, FFILinkError> linkFFIs(
    ir::Module& mod, std::span<std::filesystem::path const> libs);

} // namespace scatha::irgen

#endif // SCATHA_IRGEN_FFILINKER_H_
