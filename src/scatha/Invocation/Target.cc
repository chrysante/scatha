#include "Invocation/Target.h"

#include "Common/FileHandling.h"
#include "Invocation/ExecutableWriter.h"
#include "Invocation/TargetNames.h"
#include "Sema/Serialize.h"
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

static bool extEq(std::filesystem::path const& ext, std::string_view ref) {
    auto strext = ext.string();
    SC_EXPECT(strext.starts_with("."));
    return std::string_view(strext).substr(1) == ref;
}

std::optional<Target> Target::ReadFromDisk(std::filesystem::path const& path) {
    if (!path.has_extension() ||
        !extEq(path.extension(), TargetNames::BinaryExt))
    {
        return std::nullopt;
    }
    auto archive = Archive::Open(path);
    if (!archive) {
        return std::nullopt;
    }
    auto code = archive->openBinaryFile(TargetNames::ExecutableName);
    if (!code) {
        return std::nullopt;
    }
    auto symtxt = archive->openTextFile(TargetNames::SymbolTableName);
    sema::SymbolTable sym;
    if (!symtxt || !sema::deserialize(sym, *symtxt)) {
        return std::nullopt;
    }
    auto debugInfo = archive->openTextFile(TargetNames::DebugInfoName);
    return Target(TargetType::BinaryOnly, path.stem().string(),
                  std::make_unique<sema::SymbolTable>(std::move(sym)), *code,
                  debugInfo.value_or(std::string{}));
}

static std::string serializeToString(sema::SymbolTable const& sym) {
    std::stringstream sstr;
    sema::serialize(sym, sstr);
    return std::move(sstr).str();
}

void Target::writeToDisk(std::filesystem::path const& dir) const {
    std::filesystem::path outFile = dir / name();
    switch (type()) {
    case TargetType::Executable:
        writeExecutableFile(outFile, binary(), { .executable = true });
        if (!debugInfo().empty()) {
            auto file =
                createOutputFile(appendExt(outFile, "scdsym"), std::ios::trunc);
            file << debugInfo();
        }
        break;
    case TargetType::BinaryOnly: {
        auto archive =
            Archive::Create(appendExt(outFile, TargetNames::BinaryExt));
        /// TODO: Implement proper error handling here
        SC_RELASSERT(archive, "Failed to create archive");
        archive->addBinaryFile(TargetNames::ExecutableName, binary());
        archive->addTextFile(TargetNames::SymbolTableName,
                             serializeToString(symbolTable()));
        if (!debugInfo().empty()) {
            archive->addTextFile(TargetNames::DebugInfoName, debugInfo());
        }
        break;
    }
    case TargetType::StaticLibrary: {
        auto archive =
            Archive::Create(appendExt(outFile, TargetNames::LibraryExt));
        /// TODO: Implement proper error handling here
        SC_RELASSERT(archive, "Failed to create archive");
        archive->addTextFile(TargetNames::SymbolTableName,
                             staticLib().symbolTable);
        archive->addTextFile(TargetNames::ObjectCodeName,
                             staticLib().objectCode);
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
