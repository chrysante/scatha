#include "Sema/SemaIssue2.h"

#include <ostream>

#include <utl/utility.hpp>

#include "AST/AST.h"

using namespace scatha;
using namespace sema;

BadExpression::BadExpression(ast::Expression const& expr,
                             IssueSeverity severity):
    SemanticIssue(expr.token().sourceLocation(), severity), _expr(&expr) {}

BadTypeConversion::BadTypeConversion(ast::Expression const& expression,
                                     TypeID to):
    BadExpression(expression, IssueSeverity::Error),
    _from(expression.typeID()),
    _to(to) {}

BadOperandForUnaryExpression::BadOperandForUnaryExpression(
    ast::Expression const& expression, TypeID operand):
    BadExpression(expression, IssueSeverity::Error), _operand(operand) {}

InvalidStatement::InvalidStatement(ast::Statement const* statement,
                                   Reason reason,
                                   Scope const& inScope):
    SemanticIssue(statement ? statement->token().sourceLocation() :
                              SourceLocation{},
                  IssueSeverity::Error),
    _statement(statement),
    _reason(reason),
    _scope(&inScope) {}

void InvalidStatement::setStatement(ast::Statement const& statement) {
    _statement = &statement;
    setSourceLocation(statement.token().sourceLocation());
}

std::ostream& sema::operator<<(std::ostream& str, BadFunctionCall::Reason r) {
    // clang-format off
    return str << UTL_SERIALIZE_ENUM(r, {
        { BadFunctionCall::Reason::NoMatchingFunction, "No matching function" },
        { BadFunctionCall::Reason::ObjectNotCallable,  "Object not callable" },
    }); // clang-format on
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
    }); // clang-format on
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
        { InvalidDeclaration::Reason::ReservedIdentifier,
          "Reserved identifier" },
    }); // clang-format on
}

StrongReferenceCycle::StrongReferenceCycle(utl::vector<Node> cycle):
    SemanticIssue(cycle.front().astNode->token().sourceLocation(),
                  IssueSeverity::Error),
    _cycle(std::move(cycle)) {}
