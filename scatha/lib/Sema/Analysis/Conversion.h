#ifndef SCATHA_SEMA_ANALYSIS_CONVERSION_H_
#define SCATHA_SEMA_ANALYSIS_CONVERSION_H_

#include <iosfwd>
#include <optional>
#include <span>
#include <string_view>
#include <variant>

#include <utl/function_view.hpp>
#include <utl/overload.hpp>
#include <utl/vector.hpp>

#include "AST/Fwd.h"
#include "Common/Expected.h"
#include "Common/UniquePtr.h"
#include "Issue/IssueHandler.h"
#include "Sema/Fwd.h"
#include "Sema/QualType.h"
#include "Sema/SemaIssues.h"

namespace scatha::sema {

namespace internal {
enum class ConversionState : char { Noop, Error, Success };
} // namespace internal

/// Tag representing a conversion kind that does nothing
inline constexpr auto ConvNoop = internal::ConversionState::Noop;

/// Tag  representing a conversion error
inline constexpr auto ConvError = internal::ConversionState::Error;

/// Tag  representing a successful conversion
inline constexpr auto ConvSuccess = internal::ConversionState::Success;

/// Badly named functor that adds two states to the `Conv` type: one one noop
/// state and one error state
template <typename Conv>
class ConvExp {
public:
    struct Noop {};
    struct Error {};

    /// Construct a noop or a conversion error
    constexpr ConvExp(internal::ConversionState state): val(makeVal(state)) {}

    /// Construct from a conversion
    constexpr ConvExp(Conv conv): val(conv) {}

    /// \Returns the engaged state
    constexpr internal::ConversionState state() const {
        return internal::ConversionState(val.index());
    }

    /// \Returns `true` if this object holds a conversion
    constexpr bool hasValue() const {
        return std::holds_alternative<Conv>(val);
    }

    /// \Returns `hasValue()`
    constexpr explicit operator bool() const { return hasValue(); }

    /// \Returns `true` if this object holds a noop-conversion
    constexpr bool isNoop() const { return std::holds_alternative<Noop>(val); }

    /// \Returns `true` if this object holds a conversion error
    constexpr bool isError() const {
        return std::holds_alternative<Error>(val);
    }

    /// \Returns the value of the conversion kind.
    /// \Pre requires `hasValue()` to be `true`
    constexpr Conv value() const {
        SC_EXPECT(hasValue());
        return std::get<Conv>(val);
    }

    /// # Monadic operations

    /// \Returns the contained value if conversion was sucessful or \p fallback
    /// otherwise
    constexpr Conv valueOr(Conv fallback) const {
        return hasValue() ? value() : fallback;
    }

    /// \Overload for lazily constructed fallback
    template <std::invocable F>
    constexpr Conv valueOr(F defaultValue) const
        requires std::same_as<std::invoke_result_t<F>, Conv>
    {
        return hasValue() ? value() : std::invoke(defaultValue);
    }

    ///
    template <std::invocable<Conv> F>
    constexpr ConvExp<std::invoke_result_t<F, Conv>> transform(F&& f) const {
        using R = ConvExp<std::invoke_result_t<F, Conv>>;
        // clang-format off
        return std::visit(utl::overload{
            [&](Conv const& conv) -> R { return std::invoke(f, conv); },
            [&](Noop) -> R { return ConvNoop; },
            [&](Error) -> R { return ConvError; },
        }, val); // clang-format on
    }

    ///
    template <typename F>
        requires std::invocable<F, Noop> && std::invocable<F, Error> &&
                 std::invocable<F, Conv>
    constexpr decltype(auto) visit(F&& f) const {
        return std::visit(std::forward<F>(f), val);
    }

    ///
    bool operator==(Conv const& rhs) const {
        return hasValue() && value() == rhs;
    }

    ///
    bool operator==(ConvExp const& rhs) const { return val == rhs.val; }

private:
    constexpr static std::variant<Noop, Error, Conv> makeVal(
        internal::ConversionState state) {
        switch (state) {
        case ConvNoop:
            return Noop{};
        case ConvError:
            return Error{};
        default:
            SC_UNREACHABLE();
        }
    }

    std::variant<Noop, Error, Conv> val;
};

/// Represents a conversion of a value from one type to another
class Conversion {
public:
    Conversion(QualType fromType, QualType toType,
               std::optional<ValueCatConversion> valueCatConv,
               std::optional<MutConversion> mutConv,
               std::span<ObjectTypeConversion const> objConvs):
        from(fromType),
        to(toType),
        valueCatConv(valueCatConv),
        mutConv(mutConv),
        objConvs(objConvs | ToSmallVector<8>) {}

    /// The type of the value before the conversion
    QualType originType() const { return from; }

    /// The type of the value after the conversion
    QualType targetType() const { return to; }

    /// The conversion between value categories.
    /// This only differs from `None` if no object conversion occurs
    std::optional<ValueCatConversion> valueCatConversion() const {
        return valueCatConv;
    }

    /// The mutability conversion kind
    std::optional<MutConversion> mutConversion() const { return mutConv; }

    /// The object conversion kind
    std::span<ObjectTypeConversion const> objectConversions() const {
        return objConvs;
    }

    /// \Returns `true` if all conversions are `std::nullopt`
    bool isNoop() const {
        return !valueCatConversion() && !mutConversion() &&
               objectConversions().empty();
    }

private:
    QualType from;
    QualType to;
    /// All conversion enums are one byte in size so due to padding we don't
    /// waste space by directly storing optionals
    std::optional<ValueCatConversion> valueCatConv;
    std::optional<MutConversion> mutConv;
    utl::small_vector<ObjectTypeConversion, 8> objConvs;
};

/// Different kinds of conversion, used to select appropriate conversion
/// functions
enum class ConversionKind {
    Implicit,
    Explicit,
    Reinterpret,
};

/// Computes the conversion of \p expr to type \p toType and value category
/// \p toValueCat
SCTEST_API Expected<Conversion, std::unique_ptr<SemaIssue>> computeConversion(
    ConversionKind kind, ast::Expression const* expr, QualType toType,
    ValueCategory toValueCat);

/// Computes the rank of the conversion \p conv
SCTEST_API int computeRank(Conversion const& conv);

/// Does nothing if `expr->type() == to`
/// If \p expr is convertible of kind \p kind to type \p to a `Conversion` node
/// is inserted into the AST. Otherwise an error is pushed to \p issueHandler
/// \Returns `true` if conversion succeeded
SCTEST_API ast::Expression* convert(ConversionKind kind, ast::Expression* expr,
                                    QualType to, ValueCategory toValueCat,
                                    CleanupStack& dtors, AnalysisContext& ctx);

/// Find the common type of \p a and \p b
SCTEST_API QualType commonType(SymbolTable& symbolTable, QualType a,
                               QualType b);

/// Find the common type of \p types
SCTEST_API QualType commonType(SymbolTable& symbolTable,
                               std::span<QualType const> types);

/// Find the common type of \p expressions
SCTEST_API QualType
    commonType(SymbolTable& symbolTable,
               std::span<ast::Expression const* const> expressions);

/// Computes which AST node must be inserted to model the construction of an
/// object of \p targetType from arguments of types \p argTypes \Param kind Must
/// be `Implicit` or `Explicit`. Implicit constructions are default construction
/// and copy construction, all other constructions are only selected if this is
/// `Explicit` \Returns the computed AST node type or nullopt if construction is
/// not possible
SCATHA_API ConvExp<ObjectTypeConversion> computeObjectConstruction(
    ConversionKind kind, ObjectType const* targetType,
    std::span<ThinExpr const> arguments);

/// Allocates an AST construct expr node of type \p nodeType
SCATHA_API UniquePtr<ast::ConstructBase> allocateObjectConstruction(
    ObjectTypeConversion conv, SourceRange sourceRng,
    ObjectType const* targetType,
    utl::small_vector<UniquePtr<ast::Expression>> arguments);

/// Tries to construct an object of type \p targetType in place of the
/// expression \p replace from the arguments \p arguments
///
/// If construction is  possible, the arguments are extracted and added as
/// children to the new construct expression.
/// The construct expression will replace the expression \p replace
///
/// \Returns a pointer to the new construct expression if the  construction was
/// inserted or null pointer otherwise
SCATHA_API ast::Expression* constructInplace(
    ConversionKind kind, ast::Expression* replace, ObjectType const* targetType,
    std::span<ast::Expression* const> arguments, CleanupStack& cleanups,
    AnalysisContext& ctx);

/// \Overload for arbitrary insertion
SCATHA_API ConvExp<ast::Expression*> constructInplace(
    ConversionKind kind, ast::ASTNode const* parentNode,
    utl::function_view<ast::Expression*(UniquePtr<ast::Expression>)> insert,
    ObjectType const* targetType, std::span<ast::Expression* const> arguments,
    CleanupStack& cleanups, AnalysisContext& ctx);

/// Inserts an AST conversion node into the position of \p expr and makes
/// \p expr a child of the new node.
/// \Returns a pointer to the added conversion node
SCTEST_API ast::Expression* insertConversion(ast::Expression* expr,
                                             sema::Conversion conv,
                                             CleanupStack& dtors,
                                             AnalysisContext& ctx);

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYSIS_CONVERSION_H_
