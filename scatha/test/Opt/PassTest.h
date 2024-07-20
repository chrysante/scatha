#ifndef SCATHA_TEST_PASSTEST_H_
#define SCATHA_TEST_PASSTEST_H_

#include <functional>
#include <string>

#include "IR/Fwd.h"
#include "IR/Pass.h"
#include "IR/Pipeline.h"

namespace scatha::test {

void passTest(ir::LocalPass pass, ir::Context& fCtx, ir::Function& F,
              ir::Function& ref);

void passTest(ir::LocalPass pass, std::string F, std::string ref);

void passTest(ir::Pipeline const& pipeline, ir::Context& mCtx, ir::Module& M,
              ir::Module& ref);

void passTest(ir::Pipeline const& pipeline, std::string mSource,
              std::string refSource);

void passTest(ir::GlobalPass pass, ir::LocalPass local, std::string mSource,
              std::string refSource);

void passTest(std::string pipeline, std::string mSource, std::string refSource);

} // namespace scatha::test

#endif // SCATHA_TEST_PASSTEST_H_
