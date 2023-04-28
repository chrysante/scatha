#include "Util.h"

#include <iostream>

#include <termfmt/termfmt.h>

using namespace playground;

static size_t const width = 80;

void playground::line(std::string_view m) {
    auto impl = [](size_t width) {
        tfmt::FormatGuard grey(tfmt::brightGrey);
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
        std::cout << " " << tfmt::format(tfmt::bold, m) << " ";
        impl(right);
        std::cout << std::endl;
    }
};

void playground::header(std::string_view title) {
    std::cout << "\n";
    line("");
    line(title);
    line("");
    std::cout << "\n";
}

void playground::subHeader(std::string_view title) {
    std::cout << "\n";
    line(title);
    std::cout << "\n";
}
