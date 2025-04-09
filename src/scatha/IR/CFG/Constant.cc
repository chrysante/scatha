#include "IR/CFG/Constant.h"

#include "IR/CFG/Constants.h"
#include "IR/CFG/Function.h"
#include "IR/CFG/GlobalVariable.h"

using namespace scatha;
using namespace ir;

void Constant::writeValueTo(
    void* dest,
    utl::function_view<void(Constant const*, void*)> callback) const {
    visit(*this,
          [&](auto& derived) { derived.writeValueToImpl(dest, callback); });
}
