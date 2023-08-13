#ifndef SCATHA_PASSTOOL_PARSER_H_
#define SCATHA_PASSTOOL_PARSER_H_

#include <filesystem>
#include <utility>

#include <scatha/IR/Context.h>
#include <scatha/IR/Module.h>

namespace scatha::passtool {

enum class ParseMode { Default, Scatha, IR };

std::pair<ir::Context, ir::Module> parseFile(
    std::filesystem::path path, ParseMode mode = ParseMode::Default);

} // namespace scatha::passtool

#endif // SCATHA_PASSTOOL_PARSER_H_
