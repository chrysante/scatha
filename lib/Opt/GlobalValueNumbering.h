#ifndef SCATHA_OPT_GLOBALVALUENUMBERING_H_
#define SCATHA_OPT_GLOBALVALUENUMBERING_H_

#include "IR/Fwd.h"

namespace scatha::opt {

SCATHA_API bool globalValueNumbering(ir::Context& context,
                                     ir::Function& function);

} // namespace scatha::opt

#endif // SCATHA_OPT_GLOBALVALUENUMBERING_H_
