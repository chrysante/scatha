#include "Sema/Analysis/OverloadResolution.h"

#include <optional>
#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <variant>

#include "AST/AST.h"
#include "Sema/Analysis/Conversion.h"
#include "Sema/Analysis/Utility.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"
#include "Sema/ThinExpr.h"

using namespace scatha;
using namespace sema;
using enum ValueCategory;

/// We return a variant because we can fail in several ways:
/// If the function is not invocable with the given set of arguments we return
/// an `ORMatchError`. This does not mean that overload resolution failed, only
/// that the current overload is not a match. One of the arguments or parameters
/// may be null because of prior errors, then we return monostate. In the good
/// case we return the conversion rank
static std::variant<int, ORMatchError, std::monostate> signatureMatch(
    OverloadResolutionResult& result,
    std::span<ast::Expression const* const> args,
    std::span<Type const* const> paramTypes, ORKind kind) {
    using enum ORMatchError::Reason;
    if (args.size() != paramTypes.size()) {
        return ORMatchError{ .reason = CountMismatch };
    }
    int maxRank = 0;
    for (auto [index, expr, paramType]:
         ranges::views::zip(ranges::views::iota(size_t(0)), args, paramTypes))
    {
        if (!expr->type() || !paramType) {
            return std::monostate{};
        }
        using enum ConversionKind;
        auto convKind =
            kind == ORKind::MemberFunction && index == 0 ? Explicit : Implicit;
        auto conversion = computeConversion(convKind, expr,
                                            getQualType(paramType),
                                            refToLValue(paramType));
        if (!conversion) {
            return ORMatchError{ .reason = NoArgumentConversion,
                                 .argIndex = index };
        }
        result.conversions.push_back(*conversion);
        int rank = computeRank(*conversion);
        maxRank = std::max(maxRank, rank);
    }
    return maxRank;
}

/// Converts a list of expressions to `ThinExpr`s
static std::vector<ThinExpr> makeArgTypes(
    std::span<ast::Expression const* const> arguments) {
    return arguments | ranges::to<std::vector<ThinExpr>>;
}

OverloadResolutionResult sema::performOverloadResolution(
    ast::Expression const* parentExpr, std::span<Function* const> overloadSet,
    std::span<ast::Expression const* const> arguments, ORKind kind) {
    SC_EXPECT(!overloadSet.empty());
    utl::small_vector<OverloadResolutionResult, 4> results;
    /// Contains ranks and the index of the matching result structure in
    /// `results`
    struct RankIndexPair {
        int rank;
        uint32_t index;
    };
    utl::small_vector<RankIndexPair> ranks;
    std::unordered_map<Function const*, ORMatchError> matchErrors;
    for (auto* F: overloadSet) {
        OverloadResolutionResult match{ .function = F };
        auto rank = signatureMatch(match, arguments, F->argumentTypes(), kind);
        // clang-format off
        std::visit(utl::overload{
            [&](int rank) {
                /// `F` is a match
                ranks.push_back({ rank, utl::narrow_cast<uint32_t>(results.size()) });
                results.push_back(std::move(match));
            },
            [&](ORMatchError const& error) {
                matchErrors[F] = error;
            },
            [&](std::monostate) {
                /// We are a transitive error, just ignore this here
            },
        }, rank); // clang-format on
    }
    if (results.empty()) {
        return OverloadResolutionResult{
            .error = std::make_unique<ORError>(
                ORError::makeNoMatch(parentExpr, overloadSet,
                                     makeArgTypes(arguments), matchErrors))
        };
    }
    if (results.size() == 1) {
        return std::move(results.front());
    }
    ranges::sort(ranks, ranges::less{}, [](auto pair) { return pair.rank; });
    int const lowestRank = ranks.front().rank;
    /// TODO: Use lower_bound or similar here
    auto firstHigherRank = ranges::find_if(ranks, [=](auto pair) {
        return pair.rank != lowestRank;
    });
    ranks.erase(firstHigherRank, ranks.end());

    SC_ASSERT(!ranks.empty(),
              "ranks.size() == results.size() and results.empty() is handled "
              "above");

    /// Overload resolution succeeded
    if (ranks.size() == 1) {
        return std::move(results[ranks.front().index]);
    }

    /// Ambiguous call
    auto functions = results |
                     ranges::views::transform([](auto& r) -> auto const* {
        return r.function;
    }) | ranges::to<std::vector>;
    return OverloadResolutionResult{
        .error = std::make_unique<ORError>(
            ORError::makeAmbiguous(parentExpr, overloadSet,
                                   makeArgTypes(arguments),
                                   std::move(functions)))
    };
}
