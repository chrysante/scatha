#ifndef SCATHA_COMMON_TREEFORMATTER_H_
#define SCATHA_COMMON_TREEFORMATTER_H_

#include <cstdint>
#include <ostream>
#include <string_view>

#include <utl/streammanip.hpp>
#include <utl/vector.hpp>

namespace scatha {

enum class Level : uint8_t { Free, Occupied, Child, LastChild };

std::string_view toString(Level level);

struct TreeFormatter {
    void push(Level level);

    void pop();

    utl::vstreammanip<> beginLine();

private:
    utl::small_vector<Level> levels;
};

} // namespace scatha

#endif // SCATHA_COMMON_TREEFORMATTER_H_
