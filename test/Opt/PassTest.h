#ifndef SCATHA_TEST_PASSTEST_H_
#define SCATHA_TEST_PASSTEST_H_

#include <functional>
#include <string>

#include "IR/Fwd.h"
#include "Opt/Pass.h"

namespace scatha::test {

void passTest(opt::LocalPass pass,
              ir::Context& fCtx,
              ir::Function& F,
              ir::Function& ref);

void passTest(opt::LocalPass pass, std::string F, std::string ref);

void passTest(opt::GlobalPass pass,
              opt::LocalPass local,
              ir::Context& mCtx,
              ir::Module& M,
              ir::Module& ref);

void passTest(opt::GlobalPass pass,
              opt::LocalPass local,
              std::string F,
              std::string ref);

} // namespace scatha::test

#endif // SCATHA_TEST_PASSTEST_H_
