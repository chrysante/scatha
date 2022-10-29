#include "Sema/OverloadSet.h"

namespace scatha::sema {

Function const* OverloadSet::find(std::span<TypeID const> argumentTypes) const {
    auto const itr = functions.find(argumentTypes);
    if (itr == functions.end()) {
        return nullptr;
    }
    return *itr;
}

std::pair<Function*, bool> OverloadSet::add(Function* f) {
    SC_ASSERT(f->name() == name(), "Name of function must match name of overload set");
    auto const [itr, success] = functions.insert(f);
    return { *itr, success };
}

} // namespace scatha::sema
