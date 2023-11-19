#ifndef SDB_MODEL_SOURCEDEBUGINFO_H_
#define SDB_MODEL_SOURCEDEBUGINFO_H_

#include <filesystem>
#include <span>
#include <vector>

#include "Model/SourceFile.h"

namespace sdb {

///
class SourceDebugInfo {
public:
    ///
    static SourceDebugInfo Load(std::filesystem::path path);

    ///
    std::span<SourceFile const> files() const { return _files; }

    /// \Returns `true` if no source debug information is available
    bool empty() const { return _files.empty(); }

private:
    std::vector<SourceFile> _files;
};

} // namespace sdb

#endif // SDB_MODEL_SOURCEDEBUGINFO_H_
