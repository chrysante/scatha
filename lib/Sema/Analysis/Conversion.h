#ifndef SCATHA_SEMA_ANALYSIS_CONVERSION_H_
#define SCATHA_SEMA_ANALYSIS_CONVERSION_H_

#include <iosfwd>
#include <string_view>

#include "AST/Fwd.h"
#include "Common/Expected.h"
#include "Issue/IssueHandler.h"
#include "Sema/Analysis/DTorStack.h"
#include "Sema/Fwd.h"
#include "Sema/QualType.h"
#include "Sema/SemanticIssuesNEW.h"

namespace scatha::sema {

/// Conversion between reference qualifications
enum class ValueCatConversion : uint8_t {
#define SC_VALUECATCONV_DEF(Name, ...) Name,
#include "Sema/Analysis/Conversion.def"
};

std::string_view toString(ValueCatConversion conv);

std::ostream& operator<<(std::ostream& ostream, ValueCatConversion conv);

/// Conversion between reference qualifications
enum class MutConversion : uint8_t {
#define SC_MUTCONV_DEF(Name, ...) Name,
#include "Sema/Analysis/Conversion.def"
};

std::string_view toString(MutConversion conv);

std::ostream& operator<<(std::ostream& ostream, MutConversion conv);

/// Conversion between different object types
enum class ObjectTypeConversion : uint8_t {
#define SC_OBJTYPECONV_DEF(Name, ...) Name,
#include "Sema/Analysis/Conversion.def"
};

std::string_view toString(ObjectTypeConversion conv);

std::ostream& operator<<(std::ostream& ostream, ObjectTypeConversion conv);

/// Represents a conversion of a value from one type to another
class Conversion {
public:
    Conversion(QualType fromType,
               QualType toType,
               ValueCatConversion valueCatConv,
               MutConversion mutConv,
               ObjectTypeConversion objConv):
        from(fromType),
        to(toType),
        valueCatConv(valueCatConv),
        mutConv(mutConv),
        objConv(objConv) {}

    /// The type of the value before the conversion
    QualType originType() const { return from; }

    /// The type of the value after the conversion
    QualType targetType() const { return to; }

    /// The conversion between value categories
    ValueCatConversion valueCatConversion() const { return valueCatConv; }

    /// The mutability conversion kind
    MutConversion mutConversion() const { return mutConv; }

    /// The object conversion kind
    ObjectTypeConversion objectConversion() const { return objConv; }

    /// \Returns `true` if all of the specified conversions are `None`
    bool isNoop() const;

private:
    QualType from;
    QualType to;
    ValueCatConversion valueCatConv;
    MutConversion mutConv;
    ObjectTypeConversion objConv;
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
                                    DTorStack& dtors,
                                    AnalysisContext& ctx);

/// Dereference the expression \p expr to if it is a reference
/// Otherwise a no-op
[[deprecated]] SCTEST_API ast::Expression* dereference(ast::Expression* expr,
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
                                             sema::Conversion const& conv,
                                             SymbolTable& sym);

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYSIS_CONVERSION_H_
