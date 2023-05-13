#include "Sema/SemanticIssue.h"

#include <ostream>

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
                                     QualType const* to):
    BadExpression(expression, IssueSeverity::Error),
    _from(expression.type()),
    _to(to) {}

void BadTypeConversion::format(std::ostream& str) const {
    str << "Invalid type conversion from '" << from()->name() << "' to '"
        << to()->name() << "'\n";
}

BadOperandForUnaryExpression::BadOperandForUnaryExpression(
    ast::Expression const& expression, QualType const* operandType):
    BadExpression(expression, IssueSeverity::Error), _opType(operandType) {}

void BadOperandForUnaryExpression::format(std::ostream& str) const {
    str << "Invalid operand for unary expression";
}

void BadOperandsForBinaryExpression::format(std::ostream& str) const {
    str << "Invalid operands for binary expression";
}

InvalidStatement::InvalidStatement(ast::Statement const* statement,
                                   Reason reason,
                                   Scope const& inScope):
    SemanticIssue(statement ? statement->extSourceRange() : SourceRange{},
                  IssueSeverity::Error),
    _statement(statement),
    _reason(reason),
    _scope(&inScope) {}

InvalidStatement* InvalidStatement::setStatement(
    ast::Statement const& statement) {
    _statement = &statement;
    setSourceRange(statement.extSourceRange());
    return this;
}

void InvalidStatement::format(std::ostream& str) const {
    str << "Invalid statement: " << reason();
}

void BadFunctionCall::format(std::ostream& str) const {
    str << "No matching function for call to '" << overloadSet()->name() << "'";
}

std::ostream& sema::operator<<(std::ostream& str, BadFunctionCall::Reason r) {
    // clang-format off
    return str << UTL_SERIALIZE_ENUM(r, {
        { BadFunctionCall::Reason::NoMatchingFunction, "No matching function" },
        { BadFunctionCall::Reason::ObjectNotCallable,  "Object not callable" },
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

void BadSymbolReference::format(std::ostream& str) const {
    str << "Invalid symbol category: '";
    printExpression(expression(), str);
    str << "' is a " << have() << ", expected a " << expected();
}

std::ostream& sema::operator<<(std::ostream& str, InvalidStatement::Reason r) {
    // clang-format off
    return str << UTL_SERIALIZE_ENUM(r, {
        { InvalidStatement::Reason::ExpectedDeclaration,
          "Expected declaration" },
        { InvalidStatement::Reason::InvalidDeclaration,
          "Invalid declaration" },
        { InvalidStatement::Reason::InvalidScopeForStatement,
          "Invalid scope for statement" },
        { InvalidStatement::Reason::NonVoidFunctionMustReturnAValue,
          "Non-void function must return a value" },
        { InvalidStatement::Reason::VoidFunctionMustNotReturnAValue,
          "Void function must not return a value" },
    }); // clang-format on
}

void InvalidDeclaration::format(std::ostream& str) const {
    str << "Invalid declaration: " << reason();
}

std::ostream& sema::operator<<(std::ostream& str,
                               InvalidDeclaration::Reason r) {
    // clang-format off
    return str << UTL_SERIALIZE_ENUM(r, {
        { InvalidDeclaration::Reason::InvalidInCurrentScope,
          "Invalid in currentScope" },
        { InvalidDeclaration::Reason::Redefinition,
          "Redefinition" },
        { InvalidDeclaration::Reason::CantOverloadOnReturnType,
          "Can't overload on ReturnType" },
        { InvalidDeclaration::Reason::CantInferType,
          "Can't infer type" },
        { InvalidDeclaration::Reason::ExpectedReferenceInitializer,
          "Expected reference initializer" },
        { InvalidDeclaration::Reason::ReservedIdentifier,
          "Reserved identifier" },
        { InvalidDeclaration::Reason::ThisParameter,
          "Invalid `this` parameter" },
    }); // clang-format on
}

StrongReferenceCycle::StrongReferenceCycle(utl::vector<Node> cycle):
    SemanticIssue(cycle.front().astNode->extSourceRange(),
                  IssueSeverity::Error),
    _cycle(std::move(cycle)) {}

void StrongReferenceCycle::format(std::ostream& str) const {
    str << "Strong reference cycle";
}
