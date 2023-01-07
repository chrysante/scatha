#ifndef SCATHA_VM_BUILTIN_H_
#define SCATHA_VM_BUILTIN_H_

#include <utl/vector.hpp>

#include "Common/Builtin.h"
#include "VM/ExternalFunction.h"

namespace scatha::vm {

utl::vector<ExternalFunction> makeBuiltinTable();

} // namespace scatha::vm

#endif // SCATHA_VM_BUILTIN_H_
