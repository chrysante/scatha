#include "Invocation/Target.h"

#include "Common/FileHandling.h"
#include "Invocation/ExecutableWriter.h"
#include "Sema/SymbolTable.h"

using namespace scatha;

Target::Target(Target&&) noexcept = default;

Target& Target::operator=(Target&&) noexcept = default;

Target::~Target() = default;

static std::filesystem::path appendExt(std::filesystem::path p,
                                       std::string_view ext) {
    p += ".";
    p += ext;
    return p;
}

void Target::writeToDisk(std::filesystem::path const& dir) const {
    std::filesystem::path outFile = dir / name();
    switch (type()) {
    case TargetType::Executable:
        [[fallthrough]];
    case TargetType::BinaryOnly: {
        bool isExecutable = type() == TargetType::Executable;
        writeExecutableFile(outFile, binary(), { .executable = isExecutable });
        if (!debugInfo().empty()) {
            auto file =
                createOutputFile(appendExt(outFile, "scdsym"), std::ios::trunc);
            file << debugInfo();
        }
        break;
    }
    case TargetType::StaticLibrary: {
        auto symfile =
            createOutputFile(appendExt(outFile, "scsym"), std::ios::trunc);
        symfile << staticLib().symbolTable;
        auto objfile =
            createOutputFile(appendExt(outFile, "scir"), std::ios::trunc);
        objfile << staticLib().objectCode;
        break;
    }
    }
}

Target::Target(TargetType type, std::string name,
               std::unique_ptr<sema::SymbolTable> sym,
               std::vector<uint8_t> binary, std::string debugInfo):
    _type(type),
    _name(std::move(name)),
    _sym(std::move(sym)),
    _binary(std::move(binary)),
    _debugInfo(std::move(debugInfo)) {}

Target::Target(TargetType type, std::string name,
               std::unique_ptr<sema::SymbolTable> sym, StaticLib staticLib):
    _type(type),
    _name(std::move(name)),
    _sym(std::move(sym)),
    _staticLib(std::move(staticLib)) {}
