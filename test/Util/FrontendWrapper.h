#ifndef SCATHA_TEST_FRONTENDWRAPPER_H_
#define SCATHA_TEST_FRONTENDWRAPPER_H_

#include <string>
#include <utility>
#include <vector>

#include "IR/Fwd.h"

namespace scatha::test {

std::pair<ir::Context, ir::Module> makeIR(std::vector<std::string> sourceTexts);

} // namespace scatha::test

#endif // SCATHA_TEST_FRONTENDWRAPPER_H_
