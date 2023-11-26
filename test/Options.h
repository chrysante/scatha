#ifndef SCATHA_TEST_OPTIONS_H_
#define SCATHA_TEST_OPTIONS_H_

#include <string>

namespace scatha::test {

struct Options {
    bool TestPasses = false;
    bool TestIdempotency = false;
    std::string TestPipeline;
};

Options const& getOptions();

} // namespace scatha::test

#endif // SCATHA_TEST_OPTIONS_H_
