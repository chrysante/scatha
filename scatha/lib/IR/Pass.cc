#include "IR/Pass.h"

#include <cstdlib>
#include <optional>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>

using namespace scatha;
using namespace ir;
using namespace ranges::views;

static bool ciStrEq(std::string_view a, std::string_view b) {
    static constexpr auto toLower =
        transform([](char c) { return std::tolower(c); });
    return ranges::equal(a | toLower, b | toLower);
}

static std::optional<bool> matchFlag(std::string_view arg) {
    if (arg.empty()) {
        return true;
    }
    if (ciStrEq(arg, "y") || ciStrEq(arg, "yes") || ciStrEq(arg, "true")) {
        return true;
    }
    if (ciStrEq(arg, "n") || ciStrEq(arg, "no") || ciStrEq(arg, "false")) {
        return false;
    }
    return std::nullopt;
}

bool PassFlagArgument::doMatch(std::string_view arg) {
    if (auto value = matchFlag(arg)) {
        _value = *value;
        return true;
    }
    return false;
}

bool PassNumericArgument::doMatch(std::string_view arg) {
    try {
        _value = std::stod(std::string(arg));
        return true;
    }
    catch (std::logic_error const&) {
        return false;
    }
}

bool PassStringArgument::doMatch(std::string_view arg) {
    _value = std::string(arg);
    return true;
}

PassArgumentMap::Map PassArgumentMap::copyMap(Map const& map) {
    Map result;
    for (auto& [key, value]: map) {
        result.insert({ key, value->clone() });
    }
    return result;
}

ArgumentMatchResult PassArgumentMap::match(std::string_view key,
                                           std::string_view value) {
    using enum ArgumentMatchResult;
    auto itr = map.find(key);
    if (itr == map.end()) {
        return UnknownArgument;
    }
    if (!itr->second->match(value)) {
        return BadValue;
    }
    return Success;
}
