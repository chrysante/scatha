#include "Util/IRTestUtils.h"

#include "IR/Context.h"
#include "IR/Type.h"

using namespace scatha;
using namespace test;

ir::Type const* test::arrayPointerType(ir::Context& ctx) {
    return ctx.anonymousStruct({ ctx.ptrType(), ctx.intType(64) });
}
