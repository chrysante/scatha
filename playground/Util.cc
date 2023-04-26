#include "Util.h"

#include <iostream>

using namespace playground;

static size_t const width = 60;

void playground::line(std::string_view m) {
    auto impl = [](size_t width) {
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
        size_t left = outerSpace / 2, right = left;
        left += outerSpace % 2;
        impl(left);
        std::cout << " " << m << " ";
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
