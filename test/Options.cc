#include "test/Options.h"

using namespace scatha;
using namespace test;

static Options options{};

Options const& test::getOptions() { return options; }

namespace scatha::test {

Options& getOptionsMut() { return options; }

} // namespace scatha::test
