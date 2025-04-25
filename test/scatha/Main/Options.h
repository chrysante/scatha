#ifndef SCATHA_MAIN_OPTIONS_H_
#define SCATHA_MAIN_OPTIONS_H_

#include <string>

namespace scatha::test {

struct Options {
    bool TestPasses = false;
    bool TestIdempotency = false;
    bool PrintCodegen = false;
    bool NoJumpThreading = false;
    std::string TestPipeline;
};

Options const& getOptions();

} // namespace scatha::test

#endif // SCATHA_MAIN_OPTIONS_H_
