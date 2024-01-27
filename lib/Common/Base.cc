#include "Common/Base.h"

#include <cstdlib>
#include <iostream>
#include <string_view>

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

void internal::doAbort() { std::abort(); }

static std::string_view getHandlerName() {
    auto* name = std::getenv("SC_ASSERTION_HANDLER");
    return name ? std::string_view(name) : std::string_view();
}

internal::AssertFailureHandler internal::getAssertFailureHandler() {
    auto handler = getHandlerName();
    using enum internal::AssertFailureHandler;
    if (handler == "BREAK") {
        return Break;
    }
    if (handler == "ABORT") {
        return Abort;
    }
    if (handler == "THROW") {
        return Throw;
    }
    return Abort;
}
