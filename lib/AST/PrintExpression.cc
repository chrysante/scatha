#include "Print.h"

#include <iostream>
#include <sstream>

#include "AST/AST.h"
#include "Common/Base.h"
#include "Common/PrintUtil.h"

using namespace scatha;
using namespace ast;

namespace {

struct Context {
    void dispatchExpression(Expression const&);

    void printExpression(Identifier const&);
    void printExpression(Literal const&);
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

void Context::printExpression(Literal const& lit) {
    switch (lit.kind()) {
    case LiteralKind::Integer:
        str << lit.value<LiteralKind::Integer>().toString();
    case LiteralKind::Boolean:
        str << (lit.value<LiteralKind::Boolean>() == 1 ? "true" : "false");
    case LiteralKind::FloatingPoint:
        str << lit.value<LiteralKind::FloatingPoint>().toString();
    case LiteralKind::This:
        str << "this";
    case LiteralKind::String:
        str << '"' << lit.value<LiteralKind::String>() << '"';
    }
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
