#ifndef SCATHA_SEMA_LIFETIMEFUNCTIONANALYSIS_H_
#define SCATHA_SEMA_LIFETIMEFUNCTIONANALYSIS_H_

#include "Sema/Fwd.h"

namespace scatha::sema {

///
void analyzeLifetime(ObjectType& type, SymbolTable& sym);

} // namespace scatha::sema

#endif // SCATHA_SEMA_LIFETIMEFUNCTIONANALYSIS_H_
