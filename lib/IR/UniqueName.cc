#include "IR/UniqueName.h"

#include <range/v3/view.hpp>
#include <utl/strcat.hpp>
#include <utl/utility.hpp>

#include "Basic/Basic.h"

using namespace scatha;
using namespace ir;

/// \returns Iterator to the begin of the counter or `end()` if nonesuch is
/// found.
static std::string::iterator parseTrailingCounter(std::string& name) {
    for (auto itr = name.rbegin(); itr != name.rend(); ++itr) {
        if (*itr == '.') {
            /// Since deferencing the reverse iterator increments the underlying
            /// iterator, `.base()` already points to the char _after_ the dot.
            /// So if `name` ends with a dot we return the end iterator here.
            return itr.base();
        }
        if (!isdigit(*itr)) {
            break;
        }
    }
    return name.end();
}

std::string UniqueNameFactory::makeUnique(std::string name) {
    /// Empty names are never uniqued.
    if (name.empty()) {
        return name;
    }
    if (tryRegister(name)) {
        return name;
    }
    auto itr = parseTrailingCounter(name);
    /// If there is no trailing counter, we add a new one.
    if (itr == name.end()) {
        return appendCounter(std::move(name), 0);
    }
    char const* numBegin = std::to_address(itr);
    char* numEnd         = nullptr;
    long const counter   = std::strtol(numBegin, &numEnd, 10);
    SC_ASSERT(numEnd != numBegin, "Failed to parse counter");
    name.erase(itr - 1, name.end());
    return appendCounter(std::move(name), utl::narrow_cast<size_t>(counter));
}

bool UniqueNameFactory::tryRegister(std::string_view name) {
    auto itr = _knownNames.find(name);
    if (itr != _knownNames.end()) {
        return false;
    }
    _knownNames.insert(std::string(name));
    return true;
}

void UniqueNameFactory::erase(std::string_view name) {
    if (name.empty()) {
        return;
    }
    size_t const numElemsRemoved = _knownNames.erase(name);
    SC_ASSERT(numElemsRemoved == 1, "`name` was not registered");
}

void UniqueNameFactory::tryErase(std::string_view name) {
    if (name.empty()) {
        return;
    }
    _knownNames.erase(name);
}

std::string UniqueNameFactory::appendCounter(std::string name, size_t start) {
    for (size_t i = start;; ++i) {
        std::string result = utl::strcat(name, ".", i);
        if (tryRegister(result)) {
            return result;
        }
    }
}
