#ifndef SCATHA_SEMA_ANALYSISRESULT_H_
#define SCATHA_SEMA_ANALYSISRESULT_H_

#include <utl/vector.hpp>

#include <scatha/Sema/Fwd.h>

namespace scatha::sema {

/// Result structure of function `sema::analyze()`
struct AnalysisResult {
    /// Topologically sorted list of all struct types
    utl::vector<StructType const*> structDependencyOrder;

    /// List of all global declarations in the module
    utl::vector<ast::Declaration*> globals;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYSISRESULT_H_
