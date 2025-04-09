#ifndef SCATHA_INVOCATION_TARGETNAMES_H_
#define SCATHA_INVOCATION_TARGETNAMES_H_

#include <string_view>

namespace scatha {

struct TargetNames {
    static constexpr std::string_view BinaryExt = "scbin";
    static constexpr std::string_view LibraryExt = "sclib";
    static constexpr std::string_view ExecutableName = "executable";
    static constexpr std::string_view SymbolTableName = "sym.txt";
    static constexpr std::string_view DebugInfoName = "dbgsym.txt";
    static constexpr std::string_view ObjectCodeName = "code.scir";
};

} // namespace scatha

#endif // SCATHA_INVOCATION_TARGETNAMES_H_
