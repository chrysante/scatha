#define CATCH_CONFIG_RUNNER
#include <Catch/Catch2.hpp>

#include "test/Options.h"

using namespace scatha;
using namespace test;

static Options options{};

Options const& test::getOptions() { return options; }

int main(int argc, char* argv[]) {
    Catch::Session session;

    using namespace Catch::clara;
    auto cli = session.cli() |
               Opt(options.TestIdempotency, "idempotency")["--idempotency"](
                   "Run idempotency tests for the end to end test cases");

    session.cli(cli);
    int returnCode = session.applyCommandLine(argc, argv);
    if (returnCode != 0) {
        return returnCode;
    }
    return session.run();
}
