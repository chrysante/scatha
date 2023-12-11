#include <catch2/catch_session.hpp>

#include "test/Options.h"

using namespace scatha;
using namespace test;

static Options options{};

Options const& test::getOptions() { return options; }

int main(int argc, char* argv[]) {
    Catch::Session session;

    using namespace Catch::Clara;
    auto cli = session.cli() |
               Opt(options.TestPasses, "passes")["--passes"](
                   "Run pass tests for the end to end test cases") |
               Opt(options.TestIdempotency, "idempotency")["--idempotency"](
                   "Run idempotency tests for the end to end test cases") |
               Opt(options.TestPipeline, "pipeline")["--pipeline"](
                   "Run pass tests for the end to end test cases for the "
                   "specfied pipeline") |
               Opt(options.PrintCodegen, "print codegen")["--print-cg"](
                   "Print codegen pipeline state for failed test cases");

    session.cli(cli);
    int returnCode = session.applyCommandLine(argc, argv);
    if (returnCode != 0) {
        return returnCode;
    }
    return session.run();
}
