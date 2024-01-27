#ifndef SCATHA_SEMA_ANALYSIS_CONVERSION_H_
#define SCATHA_SEMA_ANALYSIS_CONVERSION_H_

#include <iosfwd>
#include <optional>
#include <string_view>

#include "AST/Fwd.h"
#include "Common/Expected.h"
#include "Common/UniquePtr.h"
#include "Issue/IssueHandler.h"
#include "Sema/Fwd.h"
#include "Sema/QualType.h"
#include "Sema/SemaIssues.h"

namespace scatha::sema {

namespace internal {
enum class ConvNoopT : char;
enum class ConvErrorT : char;
} // namespace internal

/// Tag representing a conversion kind that does nothing
inline constexpr internal::ConvNoopT ConvNoop{};

/// Tag  representing a conversion error
inline constexpr internal::ConvErrorT ConvError{};

/// Badly named functor that adds two states to the `Conv` type: one one noop
/// state and one error state
template <typename Conv>
class ConvExp {
public:
    /// Construct a noop
    ConvExp(internal::ConvNoopT): val(ConvNoop) {}

    /// Construct a conversion error
    ConvExp(internal::ConvErrorT): val(ConvError) {}

    /// Construct from a conversion
    ConvExp(Conv conv): val(conv) {}

    /// \Returns `true` if this object holds a conversion
    bool hasValue() const { return std::holds_alternative<Conv>(val); }

    /// \Returns `hasValue()`
    explicit operator bool() const { return hasValue(); }

    /// \Returns `true` if this object holds a noop-conversion
    bool isNoop() const {
        return std::holds_alternative<internal::ConvNoopT>(val);
    }

    /// \Returns `true` if this object holds a conversion error
    bool isError() const {
        return std::holds_alternative<internal::ConvErrorT>(val);
    }

    ///
    template <std::invocable<Conv> F>
    ConvExp<std::invoke_result_t<F, Conv>> transform(F&& f) const {
        using R = ConvExp<std::invoke_result_t<F, Conv>>;
        // clang-format off
        return std::visit(utl::overload{
            [&](Conv const& conv) -> R { return std::invoke(f, conv); },
            [&](internal::ConvNoopT) -> R { return ConvNoop; },
            [&](internal::ConvErrorT) -> R { return ConvError; },
        }, val); // clang-format on
    }

    /// \Returns the value of the conversion kind.
    /// \Pre requires `hasValue()` to be `true`
    Conv value() const {
        SC_EXPECT(hasValue());
        return std::get<Conv>(val);
    }

private:
    std::variant<Conv, internal::ConvNoopT, internal::ConvErrorT> val;
};

/// Represents a conversion of a value from one type to another
class Conversion {
public:
    Conversion(QualType fromType,
               QualType toType,
               std::optional<ValueCatConversion> valueCatConv,
               std::optional<MutConversion> mutConv,
               std::optional<ObjectTypeConversion> objConv):
        from(fromType),
        to(toType),
        valueCatConv(valueCatConv),
        mutConv(mutConv),
        objConv(objConv) {}

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
    std::optional<ObjectTypeConversion> objectConversion() const {
        return objConv;
    }

    /// \Returns `true` if all conversions are `std::nullopt`
    bool isNoop() const {
        return !valueCatConversion() && !mutConversion() && !objectConversion();
    }

private:
    QualType from;
    QualType to;
    /// All conversion enums are one byte in size so due to padding we don't
    /// waste space by directly storing optionals
    std::optional<ValueCatConversion> valueCatConv;
    std::optional<MutConversion> mutConv;
    std::optional<ObjectTypeConversion> objConv;
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
    ConversionKind kind,
    ast::Expression const* expr,
    QualType toType,
    ValueCategory toValueCat);

/// Computes the rank of the conversion \p conv
SCTEST_API int computeRank(Conversion const& conv);

/// Does nothing if `expr->type() == to`
/// If \p expr is convertible of kind \p kind to type \p to a `Conversion` node
/// is inserted into the AST. Otherwise an error is pushed to \p issueHandler
/// \Returns `true` if conversion succeeded
SCTEST_API ast::Expression* convert(ConversionKind kind,
                                    ast::Expression* expr,
                                    QualType to,
                                    ValueCategory toValueCat,
                                    CleanupStack& dtors,
                                    AnalysisContext& ctx);

/// Find the common type of \p a and \p b
SCTEST_API QualType commonType(SymbolTable& symbolTable,
                               QualType a,
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
    ConversionKind kind,
    ObjectType const* targetType,
    std::span<ThinExpr const> arguments);

/// Allocates an AST construct expr node of type \p nodeType
SCATHA_API UniquePtr<ast::ConstructBase> allocateObjectConstruction(
    ObjectTypeConversion conv,
    SourceRange sourceRng,
    ObjectType const* targetType,
    utl::small_vector<UniquePtr<ast::Expression>> arguments);

/// Inserts an AST conversion node into the position of \p expr and makes
/// \p expr a child of the new node.
/// \Returns a pointer to the added conversion node
SCTEST_API ast::Expression* insertConversion(ast::Expression* expr,
                                             sema::Conversion conv,
                                             CleanupStack& dtors,
                                             AnalysisContext& ctx);

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYSIS_CONVERSION_H_
