#ifndef SCATHA_VM_BUILTINS_H_
#define SCATHA_VM_BUILTINS_H_

#include <utl/vector.hpp>

#include "VM/ExternalFunction.h"

namespace scatha::vm {

enum class Builtins {
#define SC_BUILTIN_DEF(name) name,
#include "VM/Lists.def"
};

utl::vector<ExternalFunction> makeBuiltinTable();

} // namespace scatha::vm

#endif // SCATHA_VM_BUILTINS_H_
