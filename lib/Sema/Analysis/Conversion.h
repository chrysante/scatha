#ifndef SCATHA_SEMA_ANALYSIS_CONVERSION_H_
#define SCATHA_SEMA_ANALYSIS_CONVERSION_H_

#include <iosfwd>
#include <string_view>

#include "AST/Fwd.h"
#include "Issue/IssueHandler.h"
#include "Sema/Fwd.h"
#include "Sema/QualType.h"

namespace scatha::sema {

/// Conversion between reference qualifications
enum class RefConversion : uint8_t {
#define SC_REFCONV_DEF(Name, ...) Name,
#include "Sema/Analysis/Conversion.def"
};

std::string_view toString(RefConversion conv);

std::ostream& operator<<(std::ostream& ostream, RefConversion conv);

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
               RefConversion refConv,
               MutConversion mutConv,
               ObjectTypeConversion objConv):
        from(fromType),
        to(toType),
        refConv(refConv),
        mutConv(mutConv),
        objConv(objConv) {}

    /// The type of the value before the conversion
    QualType originType() const { return from; }

    /// The type of the value after the conversion
    QualType targetType() const { return to; }

    /// The reference conversion kind
    RefConversion refConversion() const { return refConv; }

    /// The mutability conversion kind
    MutConversion mutConversion() const { return mutConv; }

    /// The object conversion kind
    ObjectTypeConversion objectConversion() const { return objConv; }

    /// \Returns `true` if all of the specified conversions are `None`
    bool isNoop() const;

private:
    QualType from;
    QualType to;
    RefConversion refConv;
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

/// Computes the conversion from \p from to \p to
SCATHA_TESTAPI std::optional<Conversion> computeConversion(
    ConversionKind kind,
    QualType from,
    Value const* fromConstantValue,
    QualType to);

/// \overload with `nullptr` as the default argument for `fromConstantValue`
SCATHA_TESTAPI std::optional<Conversion> computeConversion(ConversionKind kind,
                                                           QualType from,
                                                           QualType to);

/// \overload for expressions
SCATHA_TESTAPI std::optional<Conversion> computeConversion(
    ConversionKind kind, ast::Expression* expr, QualType to);

/// Computes the rank of the conversion \p conv
SCATHA_TESTAPI int computeRank(Conversion const& conv);

/// Does nothing if `expr->type() == to`
/// If \p expr is implicitly convertible to type \p to a `Conversion` node is
/// inserted into the AST. Otherwise an error is pushed to \p issueHandler
/// \Returns `true` if implicit conversion succeeded
SCATHA_TESTAPI ast::Expression* convertImplicitly(ast::Expression* expr,
                                                  QualType to,
                                                  IssueHandler& issueHandler);

/// Does nothing if `expr->type() == to`
/// If \p expr is explicitly convertible to type \p to a `Conversion` node is
/// inserted into the AST. Otherwise an error is pushed to \p issueHandler
/// \Returns `true` if explicit conversion succeeded
SCATHA_TESTAPI ast::Expression* convertExplicitly(ast::Expression* expr,
                                                  QualType to,
                                                  IssueHandler& issueHandler);

/// Does nothing if `expr->type() == to`
/// If \p expr is interpretable as type \p to a `Conversion` node is
/// inserted into the AST. Otherwise an error is pushed to \p issueHandler
/// \Returns `true` if reinterpret conversion succeeded
SCATHA_TESTAPI ast::Expression* convertReinterpret(ast::Expression* expr,
                                                   QualType to,
                                                   IssueHandler& issueHandler);

/// Convert expression \p expr to an implicit reference
SCATHA_TESTAPI ast::Expression* convertToImplicitMutRef(
    ast::Expression* expr, SymbolTable& sym, IssueHandler& issueHandler);

SCATHA_TESTAPI ast::Expression* convertToExplicitRef(
    ast::Expression* expr, SymbolTable& sym, IssueHandler& issueHandler);

/// Dereference the expression \p expr to if it is a reference
/// Otherwise a no-op
SCATHA_TESTAPI void dereference(ast::Expression* expr, SymbolTable& sym);

/// Find the common type of \p a and \p b
SCATHA_TESTAPI QualType commonType(SymbolTable& symbolTable,
                                   QualType a,
                                   QualType b);

/// Find the common type of \p types
SCATHA_TESTAPI QualType commonType(SymbolTable& symbolTable,
                                   std::span<QualType const> types);

/// Find the common type of \p expressions
SCATHA_TESTAPI QualType
    commonType(SymbolTable& symbolTable,
               std::span<ast::Expression const* const> expressions);

/// Inserts an AST conversion node into the position of \p expr and makes \p
/// expr a child of the new node. \Returns a pointer to the added conversion
/// node
SCATHA_TESTAPI ast::Conversion* insertConversion(ast::Expression* expr,
                                                 sema::Conversion const& conv);

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYSIS_CONVERSION_H_
