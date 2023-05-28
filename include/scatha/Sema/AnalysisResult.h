#ifndef SCATHA_SEMA_ANALYSISRESULT_H_
#define SCATHA_SEMA_ANALYSISRESULT_H_

#include <vector>

#include <scatha/Sema/Fwd.h>

namespace scatha::sema {

/// Result structure of function `sema::analyze()`
struct AnalysisResult {
    std::vector<StructureType const*> structDependencyOrder;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYSISRESULT_H_
