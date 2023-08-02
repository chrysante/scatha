#include "Common/Logging.h"

#include <iostream>

#include <termfmt/termfmt.h>

using namespace scatha;

static size_t const width = 80;

void logging::line(std::string_view msg) { line(msg, std::cout); }

void logging::line(std::string_view msg, std::ostream& str) {
    auto impl = [&](size_t width) {
        tfmt::FormatGuard grey(tfmt::BrightGrey);
        for (size_t i = 0; i < width; ++i) {
            str << "=";
        }
    };
    if (msg.empty()) {
        impl(width);
        str << std::endl;
    }
    else if (width <= msg.size() + 2) {
        str << tfmt::format(tfmt::Bold, msg) << std::endl;
    }
    else {
        size_t outerSpace = width - (msg.size() + 2);
        size_t left = outerSpace / 4, right = outerSpace - left;
        impl(left);
        str << " " << tfmt::format(tfmt::Bold, msg) << " ";
        impl(right);
        str << std::endl;
    }
}

void logging::header(std::string_view title) { header(title, std::cout); }

void logging::header(std::string_view title, std::ostream& str) {
    str << "\n";
    line("", str);
    line(title, str);
    line("", str);
    str << "\n";
}

void logging::subHeader(std::string_view title) { subHeader(title, std::cout); }

void logging::subHeader(std::string_view title, std::ostream& str) {
    str << "\n";
    line(title, str);
    str << "\n";
}
