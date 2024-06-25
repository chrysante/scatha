#ifndef SCATHA_SEMA_ANALYSIS_PROTOCOLCONFORMANCE_H_
#define SCATHA_SEMA_ANALYSIS_PROTOCOLCONFORMANCE_H_

#include "Sema/Analysis/AnalysisContext.h"
#include "Sema/Fwd.h"

namespace scatha::sema {

bool analyzeProtocolConformance(AnalysisContext& ctx, RecordType& recordType);

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYSIS_PROTOCOLCONFORMANCE_H_
