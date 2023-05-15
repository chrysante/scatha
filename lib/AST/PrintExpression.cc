#include "AST/Print.h"

#include <iostream>
#include <sstream>

#include "AST/AST.h"
#include "Common/Base.h"
#include "Common/PrintUtil.h"

using namespace scatha;
using namespace ast;

namespace {

struct Context {
    void print(Expression const&);

    void printImpl(Identifier const&);
    void printImpl(Literal const&);
    void printImpl(UnaryExpression const&);
    void printImpl(BinaryExpression const&);
    void printImpl(MemberAccess const&);
    void printImpl(Conditional const&);
    void printImpl(FunctionCall const&);
    void printImpl(Subscript const&);
    void printImpl(UniqueExpression const&);
    void printImpl(GenericExpression const&);
    void printImpl(ReferenceExpression const&);
    void printImpl(Conversion const&);
    void printImpl(ListExpression const&);

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
    ctx.print(expr);
}

void Context::print(Expression const& expr) {
    visit(expr, [&](auto const& expr) { printImpl(expr); });
}

void Context::printImpl(Identifier const& id) { str << id.value(); }

void Context::printImpl(Literal const& lit) {
    switch (lit.kind()) {
    case LiteralKind::Integer:
        str << lit.value<APInt>().toString();
    case LiteralKind::Boolean:
        str << (lit.value<APInt>().test(1) ? "true" : "false");
        break;
    case LiteralKind::FloatingPoint:
        str << lit.value<APFloat>().toString();
        break;
    case LiteralKind::This:
        str << "this";
        break;
    case LiteralKind::String:
        str << '"' << lit.value<std::string>() << '"';
        break;
    case LiteralKind::Char:
        str << lit.value<APInt>().toString(16);
        break;
    }
}

void Context::printImpl(UnaryExpression const& expr) {
    str << expr.operation();
    print(*expr.operand());
}

void Context::printImpl(BinaryExpression const& expr) {
    print(*expr.lhs());
    str << expr.operation();
    print(*expr.rhs());
}

void Context::printImpl(MemberAccess const& memberAccess) {
    print(*memberAccess.object());
    str << ".";
    print(*memberAccess.member());
}

void Context::printImpl(Conditional const& conditional) {
    print(*conditional.condition());
    str << " ? ";
    print(*conditional.thenExpr());
    str << " : ";
    print(*conditional.elseExpr());
}

void Context::printImpl(FunctionCall const& functionCall) {
    print(*functionCall.object());
    str << "(";
    for (bool first = true; auto* argument: functionCall.arguments()) {
        if (!first) {
            str << ", ";
        }
        else {
            first = false;
        }
        print(*argument);
    }
    str << ")";
}

void Context::printImpl(Subscript const& subscript) {
    print(*subscript.object());
    str << "[";
    for (bool first = true; auto* argument: subscript.arguments()) {
        if (!first) {
            str << ", ";
        }
        first = false;
        print(*argument);
    }
    str << "]";
}

void Context::printImpl(UniqueExpression const& expr) { SC_UNIMPLEMENTED(); }

void Context::printImpl(GenericExpression const& expr) { SC_UNIMPLEMENTED(); }

void Context::printImpl(ReferenceExpression const& ref) {
    str << "&";
    if (ref.isMutable()) {
        str << "mut ";
    }
    print(*ref.referred());
}

void Context::printImpl(Conversion const& conv) { print(*conv.expression()); }

void Context::printImpl(ListExpression const& list) {
    str << "[";
    bool first = true;
    for (auto* expr: list.elements()) {
        if (!first) {
            str << ", ";
        }
        first = false;
        print(*expr);
    }
    str << "]";
}
