#include "Util.h"

#include <iostream>
#include <termfmt/termfmt.h>

using namespace scatha;
using namespace passtool;

void passtool::line(std::string_view m) {
    static constexpr size_t width = 80;
    auto impl = [](size_t width) {
        tfmt::FormatGuard grey(tfmt::BrightGrey);
        for (size_t i = 0; i < width; ++i) {
            std::cout << "=";
        }
    };
    if (m.empty()) {
        impl(width);
        std::cout << std::endl;
    }
    else if (width <= m.size() + 2) {
        std::cout << m << std::endl;
    }
    else {
        size_t outerSpace = width - (m.size() + 2);
        size_t left = outerSpace / 4, right = outerSpace - left;
        impl(left);
        std::cout << " " << tfmt::format(tfmt::Bold, m) << " ";
        impl(right);
        std::cout << std::endl;
    }
}

void passtool::header(std::string_view title) {
    std::cout << "\n";
    line("");
    line(title);
    line("");
    std::cout << "\n";
}

void passtool::subHeader(std::string_view title) {
    std::cout << "\n";
    line(title);
    std::cout << "\n";
}

extern utl::vstreammanip<> const passtool::Warning([](std::ostream& str) {
    str << tfmt::format(tfmt::Yellow | tfmt::Bold, "Warning: ");
});

extern utl::vstreammanip<> const passtool::Error([](std::ostream& str) {
    str << tfmt::format(tfmt::Red | tfmt::Bold, "Error: ");
});
