#ifndef SCATHA_TEST_PASSTEST_H_
#define SCATHA_TEST_PASSTEST_H_

#include <functional>
#include <string>

#include "IR/Fwd.h"

namespace scatha::test {

void passTest(std::function<bool(ir::Context&, ir::Function&)> pass,
              ir::Context& fCtx,
              ir::Function& F,
              ir::Function& ref);

void passTest(std::function<bool(ir::Context&, ir::Function&)> pass,
              std::string F,
              std::string ref);

} // namespace scatha::test

#endif // SCATHA_TEST_PASSTEST_H_
