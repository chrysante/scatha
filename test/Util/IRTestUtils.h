#ifndef SCATHA_TEST_IRTESTUTILS_H_
#define SCATHA_TEST_IRTESTUTILS_H_

#include <concepts>
#include <stdexcept>

#include "IR/CFG/BasicBlock.h"

namespace scatha::test {

/// \Returns the `{ ptr, i64 }` anonymous struct type
ir::Type const* arrayPointerType(ir::Context& ctx);

/// Helper class that can be used to successively check every instruction in a
/// basic block
struct BBView {
    ir::BasicBlock::ConstIterator itr;
    ir::BasicBlock const* BB;

    explicit BBView(ir::BasicBlock const& BB): itr(BB.begin()), BB(&BB) {}

    template <std::derived_from<ir::Instruction> Inst>
    Inst const& nextAs() {
        if (itr == BB->end()) {
            throw std::runtime_error("Reached end of basic block");
        }
        return dyncast<Inst const&>(*itr++);
    }

    template <std::derived_from<ir::Instruction> Inst>
    bool nextIs() {
        return nothrow([this] { nextAs<Inst>(); });
    }

    template <std::derived_from<ir::TerminatorInst> Term>
    Term const& terminatorAs() const {
        auto* term = BB->terminator<Term>();
        if (!term) {
            throw std::runtime_error("Block has no terminator");
        }
        return *term;
    }

    template <std::derived_from<ir::TerminatorInst> Term>
    bool terminatorIs() const {
        return nothrow([this] { terminatorAs<Term>(); });
    }

    BBView nextBlock() const { return BBView(*BB->next()); }

private:
    static bool nothrow(auto f) {
        try {
            f();
            return true;
        }
        catch (...) {
            return false;
        }
    }
};

} // namespace scatha::test

#endif // SCATHA_TEST_IRTESTUTILS_H_
