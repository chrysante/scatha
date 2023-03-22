#include "Print.h"

#include <iostream>
#include <sstream>

#include "AST/AST.h"
#include "Basic/Basic.h"
#include "Basic/PrintUtil.h"

using namespace scatha;
using namespace ast;

namespace {

struct Context {
    void dispatchExpression(Expression const&);

    void printExpression(Identifier const&);
    void printExpression(IntegerLiteral const&);
    void printExpression(BooleanLiteral const&);
    void printExpression(FloatingPointLiteral const&);
    void printExpression(StringLiteral const&);
    void printExpression(UnaryPrefixExpression const&);
    void printExpression(BinaryExpression const&);
    void printExpression(MemberAccess const&);
    void printExpression(Conditional const&);
    void printExpression(FunctionCall const&);
    void printExpression(Subscript const&);
    void printExpression(AbstractSyntaxTree const&) { SC_UNREACHABLE(); }

    std::ostream& str;
};

} // namespace

std::string ast::toString(Expression const& expr) {
    std::stringstream sstr;
    printExpression(expr, sstr);
    return sstr.str();
}

void ast::printExpression(Expression const& expr) {
    printExpression(expr, std::cout);
}

void ast::printExpression(Expression const& expr, std::ostream& str) {
    Context ctx{ str };
    ctx.dispatchExpression(expr);
}

void Context::dispatchExpression(Expression const& expr) {
    visit(expr, [&](auto const& expr) { printExpression(expr); });
}

void Context::printExpression(Identifier const& id) { str << id.value(); }

void Context::printExpression(IntegerLiteral const& l) { str << l.token().id; }

void Context::printExpression(BooleanLiteral const& l) { str << l.token().id; }

void Context::printExpression(FloatingPointLiteral const& l) {
    str << l.token().id;
}

void Context::printExpression(StringLiteral const& l) {
    str << '"' << l.token().id << '"';
}

void Context::printExpression(UnaryPrefixExpression const& expr) {
    str << expr.operation();
    dispatchExpression(*expr.operand);
}

void Context::printExpression(BinaryExpression const& expr) {
    dispatchExpression(*expr.lhs);
    str << expr.operation();
    dispatchExpression(*expr.rhs);
}

void Context::printExpression(MemberAccess const& memberAccess) {
    dispatchExpression(*memberAccess.object);
    str << ".";
    dispatchExpression(*memberAccess.member);
}

void Context::printExpression(Conditional const& conditional) {
    dispatchExpression(*conditional.condition);
    str << " ? ";
    dispatchExpression(*conditional.ifExpr);
    str << " : ";
    dispatchExpression(*conditional.elseExpr);
}

void Context::printExpression(FunctionCall const& functionCall) {
    dispatchExpression(*functionCall.object);
    str << "(";
    for (bool first = true; auto& argument: functionCall.arguments) {
        if (!first) {
            str << ", ";
        }
        else {
            first = false;
        }
        dispatchExpression(*argument);
    }
    str << ")";
}

void Context::printExpression(Subscript const& subscript) {
    dispatchExpression(*subscript.object);
    str << "[";
    for (bool first = true; auto& argument: subscript.arguments) {
        if (!first) {
            str << ", ";
        }
        else {
            first = false;
        }
        dispatchExpression(*argument);
    }
    str << "]";
}
