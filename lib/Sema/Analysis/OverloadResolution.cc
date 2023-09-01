#include "Sema/Analysis/OverloadResolution.h"

#include <optional>
#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>

#include "AST/AST.h"
#include "Sema/Analysis/Conversion.h"
#include "Sema/Entity.h"

using namespace scatha;
using namespace sema;

///  \returns The maximum conversion rank if all arguments are convertible to
/// the parameters  `std::nullopt` otherwise
static std::optional<int> signatureMatch(
    std::span<QualType const> argTypes,
    std::span<Value const* const> constantArgs,
    std::span<QualType const> paramTypes,
    bool isMemberCall) {
    if (paramTypes.size() != argTypes.size()) {
        return std::nullopt;
    }
    SC_ASSERT(argTypes.size() == constantArgs.size(), "");
    int maxRank = 0;
    for (auto [index, argType, constArg, paramType]:
         ranges::views::zip(ranges::views::iota(0),
                            argTypes,
                            constantArgs,
                            paramTypes))
    {
        if (!paramType || !argType) {
            return std::nullopt;
        }
        auto const rank =
            isMemberCall && index == 0 ?
                explicitConversionRank(argType, constArg, paramType) :
                implicitConversionRank(argType, constArg, paramType);
        if (!rank) {
            return std::nullopt;
        }
        maxRank = std::max(maxRank, *rank);
    }
    return maxRank;
}

namespace {

struct Match {
    Function* function;
    int rank;
};

} // namespace

static Expected<Function*, OverloadResolutionError*> performORImpl(
    OverloadSet* overloadSet,
    std::span<QualType const> argTypes,
    std::span<Value const* const> constArgs,
    bool isMemberCall) {
    utl::small_vector<Match> matches;
    for (auto* F: *overloadSet) {
        auto rank = signatureMatch(argTypes,
                                   constArgs,
                                   F->argumentTypes(),
                                   isMemberCall);
        if (rank) {
            matches.push_back({ F, *rank });
        }
    }
    if (matches.empty()) {
        return new NoMatchingFunction(overloadSet);
    }
    if (matches.size() == 1) {
        return matches.front().function;
    }
    ranges::sort(matches, ranges::less{}, [](Match match) {
        return match.rank;
    });
    int const lowestRank = matches.front().rank;
    auto firstHigherRank = ranges::find_if(matches, [=](Match match) {
        return match.rank != lowestRank;
    });
    matches.erase(firstHigherRank, matches.end());

    switch (matches.size()) {
    case 0:
        return new NoMatchingFunction(overloadSet);
    case 1:
        return matches.front().function;
    default: {
        auto functions = matches | ranges::views::transform([](Match match) {
                             return match.function;
                         }) |
                         ranges::to<utl::small_vector<Function*>>;
        return new AmbiguousOverloadResolution(overloadSet,
                                               std::move(functions));
    }
    }
}

Expected<Function*, OverloadResolutionError*> sema::performOverloadResolution(
    OverloadSet* overloadSet,
    std::span<QualType const> argTypes,
    bool isMemberCall) {
    return performORImpl(overloadSet,
                         argTypes,
                         utl::small_vector<Value const*>(argTypes.size()),
                         isMemberCall);
}

/// \overload for expressions
Expected<Function*, OverloadResolutionError*> sema::performOverloadResolution(
    OverloadSet* overloadSet,
    std::span<ast::Expression const* const> arguments,
    bool isMemberCall) {
    utl::small_vector<QualType> argTypes;
    utl::small_vector<Value const*> argValues;
    argTypes.reserve(arguments.size());
    argValues.reserve(arguments.size());
    for (auto* arg: arguments) {
        argTypes.push_back(arg->type());
        argValues.push_back(arg->constantValue());
    }
    return performORImpl(overloadSet, argTypes, argValues, isMemberCall);
}
