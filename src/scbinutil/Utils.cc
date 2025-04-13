#include "scbinutil/Utils.h"

using namespace scbinutil;

std::span<uint8_t const> scbinutil::seekBinary(std::span<uint8_t const> file) {
    auto* data = file.data();
    auto* end = std::to_address(file.end());
    /// We ignore any empty lines
    while (data < end && *data == '\n')
        ++data;
    /// We ignore lines starting with `#` and the next line
    while (data < end && *data == '#') {
        for (int i = 0; i < 2; ++i) {
            while (data < end && *data != '\n')
                ++data;
            if (data < end) ++data;
        }
    }
    return std::span(data, end);
}
