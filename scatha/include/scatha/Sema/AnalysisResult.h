#ifndef SCATHA_SEMA_ANALYSISRESULT_H_
#define SCATHA_SEMA_ANALYSISRESULT_H_

#include <utl/vector.hpp>

#include <scatha/Sema/Fwd.h>

namespace scatha::sema {

/// Result structure of function `sema::analyze()`
struct AnalysisResult {
    utl::vector<StructType const*> structDependencyOrder;
    utl::vector<ast::FunctionDefinition*> functions;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYSISRESULT_H_
