#ifndef SCATHA_TEST_IR_EQUAL_H_
#define SCATHA_TEST_IR_EQUAL_H_

#include <string>

#include <iosfwd>

#include "IR/Common.h"

namespace scatha::test {

struct EqResult {
    enum SuccessT { Success };

    EqResult(SuccessT) {}

    EqResult(scatha::ir::Value const* a,
             scatha::ir::Value const* b,
             std::string msg):
        a(a), b(b), msg(msg) {}

    EqResult(scatha::ir::Value const* a,
             scatha::ir::Value const* b,
             char const* msg):
        EqResult(a, b, std::string(msg)) {}

    bool success() const { return msg.empty(); }

    explicit operator bool() const { return success(); }

    scatha::ir::Value const* a = nullptr;
    scatha::ir::Value const* b = nullptr;
    std::string msg;
};

std::ostream& operator<<(std::ostream& str, EqResult const& eqResult);

EqResult modEqual(scatha::ir::Module const& A, scatha::ir::Module const& B);

EqResult funcEqual(scatha::ir::Function const& F,
                   scatha::ir::Function const& G);

} // namespace scatha::test

#endif // SCATHA_TEST_IR_EQUAL_H_
