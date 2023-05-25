#include "Common/TreeFormatter.h"

#include <termfmt/termfmt.h>

#include "Common/Base.h"

using namespace scatha;

std::string_view scatha::toString(Level l) {
#if SC_UNICODE_TERMINAL
    switch (l) {
    case Level::Free:
        return "   ";
    case Level::Occupied:
        return "│  ";
    case Level::Child:
        return "├─ ";
    case Level::LastChild:
        return "└─ ";
    }
#else  // FANCY_TREE_SYMBOLS
    switch (l) {
    case Level::Free:
        return "   ";
    case Level::Occupied:
        return "|  ";
    case Level::Child:
        return "|- ";
    case Level::LastChild:
        return "+- ";
    }
#endif // FANCY_TREE_SYMBOLS
}

void TreeFormatter::push(Level l) {
    if (!levels.empty() && levels.back() == Level::Child) {
        levels.back() = Level::Occupied;
    }
    levels.push_back(l);
}

void TreeFormatter::pop() { levels.pop_back(); }

utl::vstreammanip<> TreeFormatter::beginLine() {
    return [this](std::ostream& str) {
        tfmt::format(tfmt::BrightGrey, str, [&] {
            for (auto l: levels) {
                str << toString(l);
            }
        });
        if (!levels.empty() && levels.back() == Level::LastChild) {
            levels.back() = Level::Free;
        }
    };
}
