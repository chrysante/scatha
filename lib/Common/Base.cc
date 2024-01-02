#include "Common/Base.h"

#include <cstdlib>
#include <iostream>

#include <termfmt/termfmt.h>
#include <utl/streammanip.hpp>

using namespace scatha;

static constexpr utl::streammanip Fatal([](std::ostream& str, auto... args) {
    str << tfmt::format(tfmt::BGRed | tfmt::BrightWhite | tfmt::Bold,
                        "Fatal error:")
        << " " << tfmt::format(tfmt::Bold, args...);
});

static constexpr utl::streammanip formatLocation(
    [](std::ostream& str, char const* file, int line, char const* function) {
    str << "    In function: " << tfmt::format(tfmt::Italic, function) << "\n";
    str << "    On line:     " << tfmt::format(tfmt::Italic, line) << "\n";
    str << "    In file:     " << tfmt::format(tfmt::Italic, "\"", file, "\"");
});

void internal::unimplemented(char const* file, int line, char const* function) {
    std::cerr << Fatal("Hit unimplemented code path.") << "\n"
              << formatLocation(file, line, function) << "\n";
}

void internal::unreachable(char const* file, int line, char const* function) {
    std::cerr << Fatal("Hit unreachable code path.") << "\n"
              << formatLocation(file, line, function) << "\n";
}

void internal::assertionFailure(char const* file,
                                int line,
                                char const* function,
                                char const* expr,
                                char const* msg) {
    std::cerr << Fatal("Assertion failed:") << " " << expr << "\n"
              << "    With message: " << tfmt::format(tfmt::Italic, msg) << "\n"
              << formatLocation(file, line, function) << "\n";
}

void internal::relfail() { std::abort(); }
