#ifndef SCATHA_SEMA_ANALYSIS_CONVERSION_H_
#define SCATHA_SEMA_ANALYSIS_CONVERSION_H_

#include <iosfwd>
#include <string_view>

#include "AST/Fwd.h"
#include "Issue/IssueHandler.h"
#include "Sema/Fwd.h"

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
    Conversion(QualType const* fromType,
               QualType const* toType,
               RefConversion refConv,
               MutConversion mutConv,
               ObjectTypeConversion objConv):
        from(fromType),
        to(toType),
        refConv(refConv),
        mutConv(mutConv),
        objConv(objConv) {}

    /// The type of the value before the conversion
    QualType const* originType() const { return from; }

    /// The type of the value after the conversion
    QualType const* targetType() const { return to; }

    /// The reference conversion kind
    RefConversion refConversion() const { return refConv; }

    /// The mutability conversion kind
    MutConversion mutConversion() const { return mutConv; }

    /// The object conversion kind
    ObjectTypeConversion objectConversion() const { return objConv; }

private:
    QualType const* from;
    QualType const* to;
    RefConversion refConv;
    MutConversion mutConv;
    ObjectTypeConversion objConv;
};

/// Does nothing if `expr->type() == to`
/// If \p expr is implicitly convertible to type \p to a `Conversion` node is
/// inserted into the AST. Otherwise an error is pushed to \p issueHandler
/// \Returns `true` iff implicit conversion succeeded
bool convertImplicitly(ast::Expression* expr,
                       QualType const* to,
                       IssueHandler& issueHandler);

/// Does nothing if `expr->type() == to`
/// If \p expr is explicitly convertible to type \p to a `Conversion` node is
/// inserted into the AST. Otherwise an error is pushed to \p issueHandler
/// \Returns `true` iff explicit conversion succeeded
bool convertExplicitly(ast::Expression* expr,
                       QualType const* to,
                       IssueHandler& issueHandler);

/// Does nothing if `expr->type() == to`
/// If \p expr is interpretable as type \p to a `Conversion` node is
/// inserted into the AST. Otherwise an error is pushed to \p issueHandler
/// \Returns `true` iff reinterpret conversion succeeded
bool convertReinterpret(ast::Expression* expr,
                        QualType const* to,
                        IssueHandler& issueHandler);

/// \Returns The rank of the conversion if an implicit conversion from type \p
/// from to type \p to exists. Otherwise `std::nullopt`
std::optional<int> implicitConversionRank(QualType const* from,
                                          QualType const* to);

/// \Returns The rank of the conversion if an explicit conversion from type \p
/// from to type \p to exists. Otherwise `std::nullopt`
std::optional<int> explicitConversionRank(QualType const* from,
                                          QualType const* to);

/// Convert expression \p expr to an implicit reference
bool convertToImplicitRef(ast::Expression* expr,
                          SymbolTable& sym,
                          IssueHandler& issueHandler);

bool convertToExplicitRef(ast::Expression* expr,
                          SymbolTable& sym,
                          IssueHandler& issueHandler);

/// Dereference the expression \p expr to if it is a reference
/// Otherwise a no-op
void dereference(ast::Expression* expr, SymbolTable& sym);

/// Find the common type of \p a and \p b
QualType const* commonType(QualType const* a, QualType const* b);

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYSIS_CONVERSION_H_
