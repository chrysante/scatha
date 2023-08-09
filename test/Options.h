#ifndef SCATHA_TEST_OPTIONS_H_
#define SCATHA_TEST_OPTIONS_H_

namespace scatha::test {

struct Options {
    bool TestIdempotency = false;
};

Options const& getOptions();

} // namespace scatha::test

#endif // SCATHA_TEST_OPTIONS_H_
