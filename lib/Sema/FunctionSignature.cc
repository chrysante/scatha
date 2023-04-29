#include "Sema/FunctionSignature.h"

#include <range/v3/view.hpp>
#include <utl/hash.hpp>
#include <utl/utility.hpp>

#include "Sema/Entity.h"

using namespace scatha;
using namespace sema;

bool sema::argumentsEqual(FunctionSignature const& a,
                          FunctionSignature const& b) {
    return ranges::equal(a.argumentTypes(), b.argumentTypes());
}
