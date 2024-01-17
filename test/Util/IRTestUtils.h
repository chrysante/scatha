#ifndef SCATHA_TEST_BBVIEW_H_
#define SCATHA_TEST_BBVIEW_H_

#include <concepts>

#include "IR/CFG/BasicBlock.h"

namespace scatha::test {

/// \Returns the `{ ptr, i64 }` anonymous struct type
ir::Type const* arrayPointerType(ir::Context& ctx);

/// Helper class that can be used to successively check every instruction in a
/// basic blocl
struct BBView {
    ir::BasicBlock::ConstIterator itr;

    explicit BBView(ir::BasicBlock const& BB): itr(BB.begin()) {}

    template <std::derived_from<ir::Instruction> Inst>
    Inst const& nextAs() {
        return dyncast<Inst const&>(*itr++);
    }
};

} // namespace scatha::test

#endif // SCATHA_TEST_BBVIEW_H_
