#ifndef SCATHA_SEMA_ANALYSIS_CONVERSION_H_
#define SCATHA_SEMA_ANALYSIS_CONVERSION_H_

#include <iosfwd>
#include <optional>
#include <string_view>

#include "AST/Fwd.h"
#include "Common/Expected.h"
#include "Issue/IssueHandler.h"
#include "Sema/Fwd.h"
#include "Sema/QualType.h"
#include "Sema/SemaIssues.h"

namespace scatha::sema {

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

private:
    QualType from;
    QualType to;
    /// All conversion enums are one byte in size so due to padding we don't
    /// waste space by directly storing optionals
    std::optional<ValueCatConversion> valueCatConv{};
    std::optional<MutConversion> mutConv{};
    std::optional<ObjectTypeConversion> objConv{};
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
                                    DtorStack& dtors,
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

/// Inserts an AST conversion node into the position of \p expr and makes
/// \p expr a child of the new node.
/// \Returns a pointer to the added conversion node
SCTEST_API ast::Expression* insertConversion(ast::Expression* expr,
                                             sema::Conversion conv,
                                             DtorStack& dtors,
                                             AnalysisContext& ctx);

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYSIS_CONVERSION_H_
