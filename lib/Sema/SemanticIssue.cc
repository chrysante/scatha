#include "Sema/SemanticIssue.h"

#include <ostream>

#include <utl/streammanip.hpp>
#include <utl/utility.hpp>

#include "AST/AST.h"
#include "AST/Print.h"
#include "Sema/Entity.h"

using namespace scatha;
using namespace sema;

BadExpression::BadExpression(ast::Expression const& expr,
                             IssueSeverity severity):
    SemanticIssue(expr.extSourceRange(), severity), _expr(&expr) {}

void BadExpression::format(std::ostream& str) const { str << "Bad expression"; }

AssignmentToConst::AssignmentToConst(ast::Expression const& expression):
    BadExpression(expression, IssueSeverity::Error) {}

void AssignmentToConst::format(std::ostream& str) const {
    str << "Assignment to immutable value";
}

BadTypeConversion::BadTypeConversion(ast::Expression const& expression,
                                     QualType to):
    BadExpression(expression, IssueSeverity::Error),
    _from(expression.type()),
    _to(to) {}

void BadTypeConversion::format(std::ostream& str) const {
    str << "Invalid type conversion from " << from()->name() << " to "
        << to()->name() << "\n";
}

BadOperandForUnaryExpression::BadOperandForUnaryExpression(
    ast::Expression const& expression, QualType operandType):
    BadExpression(expression, IssueSeverity::Error), _opType(operandType) {}

void BadOperandForUnaryExpression::format(std::ostream& str) const {
    str << "Invalid operand for unary expression";
}

void BadOperandsForBinaryExpression::format(std::ostream& str) const {
    str << "Invalid operands for binary expression: `" << lhs()->name()
        << "` and `" << rhs()->name() << "`";
}

void BadFunctionCall::format(std::ostream& str) const {
    str << "No matching function for call to '" << overloadSet()->name()
        << "'. " << reason();
}

std::ostream& sema::operator<<(std::ostream& str, BadFunctionCall::Reason r) {
    // clang-format off
    return str << UTL_SERIALIZE_ENUM(r, {
        { BadFunctionCall::Reason::NoMatchingFunction, "No matching function" },
        { BadFunctionCall::Reason::ObjectNotCallable,  "Object not callable" },
        { BadFunctionCall::Reason::CantDeduceReturnType,
          "Cannot deduce return type on mutually recursive function" }
    }); // clang-format on
}

void UseOfUndeclaredIdentifier::format(std::ostream& str) const {
    str << "Use of undeclared identifier '" << toString(expression()) << "'";
}

BadSymbolReference::BadSymbolReference(ast::Expression const& expr,
                                       EntityCategory expected):
    BadExpression(expr, IssueSeverity::Error),
    _have(expr.entityCategory()),
    _expected(expected) {}

static constexpr utl::streammanip printEC([](std::ostream& str,
                                             EntityCategory cat) {
    using enum EntityCategory;
    switch (cat) {
    case Value:
        str << "a value";
        break;
    case Type:
        str << "a type";
        break;
    case Indeterminate:
        str << "indeterminate";
        break;
    }
});

void BadSymbolReference::format(std::ostream& str) const {
    str << "Invalid symbol category: '";
    printExpression(expression(), str);
    str << "' is " << printEC(have()) << ", expected " << printEC(expected());
}

StrongReferenceCycle::StrongReferenceCycle(utl::vector<Node> cycle):
    SemanticIssue(cycle.front().astNode->extSourceRange(),
                  IssueSeverity::Error),
    _cycle(std::move(cycle)) {}

void StrongReferenceCycle::format(std::ostream& str) const {
    str << "Strong reference cycle";
}

InvalidListExpr::InvalidListExpr(ast::ListExpression const& expr,
                                 Reason reason):
    BadExpression(expr, IssueSeverity::Error), _reason(reason) {}

void InvalidListExpr::format(std::ostream& str) const {
    str << "Invalid list expression: " << reason();
}

std::ostream& sema::operator<<(std::ostream& str, InvalidListExpr::Reason r) {
    // clang-format off
    return str << UTL_SERIALIZE_ENUM(r, {
        { InvalidListExpr::Reason::InvalidElemCountForArrayType,
          "Invalid element count for array type declaration" },
        { InvalidListExpr::Reason::InvalidArrayCount,
          "Invalid array count" },
        { InvalidListExpr::Reason::NoCommonType,
          "No common type" },
    }); // clang-format on
}

void InvalidNameLookup::format(std::ostream& str) const {
    str << "Invalid name lookup";
}

OverloadResolutionError::OverloadResolutionError(
    OverloadSet const* overloadSet):
    SemanticIssue(SourceLocation{}, IssueSeverity::Error), os(overloadSet) {}

void NoMatchingFunction::format(std::ostream& str) const {
    str << "No matching function for call to '" << overloadSet()->name() << "'";
}

void AmbiguousOverloadResolution::format(std::ostream& str) const {
    str << "Ambiguous call to '" << overloadSet()->name() << "'";
}
