#include "Invocation/Target.h"

#include "Common/FileHandling.h"
#include "Invocation/ExecutableWriter.h"

using namespace scatha;

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
        symfile << symbolTable();
        auto objfile =
            createOutputFile(appendExt(outFile, "scir"), std::ios::trunc);
        objfile << objectCode();
        break;
    }
    }
}
