#include "AST/Print.h"

#include <iostream>

#include <termfmt/termfmt.h>
#include <utl/streammanip.hpp>

#include "AST/AST.h"
#include "Common/Base.h"
#include "Common/PrintUtil.h"
#include "Sema/Entity.h"

using namespace scatha;
using namespace ast;

static constexpr char endl = '\n';

static Indenter indent(int level) { return Indenter(level, 2); }

namespace {

struct Context {
    void dispatch(AbstractSyntaxTree const*, int ind);

    void print(TranslationUnit const&, int ind);
    void print(CompoundStatement const&, int ind);
    void print(FunctionDefinition const&, int ind);
    void print(StructDefinition const&, int ind);
    void print(VariableDeclaration const&, int ind);
    void print(ParameterDeclaration const&, int ind);
    void print(ExpressionStatement const&, int ind);
    void print(EmptyStatement const&, int ind);
    void print(ReturnStatement const&, int ind);
    void print(IfStatement const&, int ind);
    void print(WhileStatement const&, int ind);
    void print(DoWhileStatement const&, int ind);
    void print(Identifier const&, int ind);
    void print(IntegerLiteral const&, int ind);
    void print(BooleanLiteral const&, int ind);
    void print(FloatingPointLiteral const&, int ind);
    void print(StringLiteral const&, int ind);
    void print(UnaryPrefixExpression const&, int ind);
    void print(BinaryExpression const&, int ind);
    void print(MemberAccess const&, int ind);
    void print(ReferenceExpression const&, int ind);
    void print(UniqueExpression const&, int ind);
    void print(Conditional const&, int ind);
    void print(FunctionCall const&, int ind);
    void print(Subscript const&, int ind);
    void print(ListExpression const&, int ind);
    void print(AbstractSyntaxTree const& node, int ind) {
        printHeader(node, ind);
    }

    void printHeader(AbstractSyntaxTree const&, int ind, auto... args);

    void printType(Expression const& expr, int ind) {
        if (expr.isDecorated() && expr.type()) {
            str << indent(ind) << tfmt::format(tfmt::brightGrey, "Type: ")
                << expr.type()->name()
                << tfmt::format(tfmt::brightGrey,
                                " [",
                                expr.valueCategory(),
                                "]")
                << endl;
        }
    }

    std::ostream& str;
};

} // namespace

void ast::printTree(AbstractSyntaxTree const& root) {
    printTree(root, std::cout);
}

void ast::printTree(AbstractSyntaxTree const& root, std::ostream& str) {
    Context ctx{ str };
    ctx.dispatch(&root, 0);
}

void Context::dispatch(AbstractSyntaxTree const* node, int ind) {
    if (node) {
        visit(*node, [&](auto const& node) { return print(node, ind); });
    }
}

void Context::print(TranslationUnit const& tu, int ind) {
    printHeader(tu, ind);
    for (auto& decl: tu.declarations) {
        dispatch(decl.get(), ind + 1);
    }
}

void Context::print(CompoundStatement const& stmt, int ind) {
    printHeader(stmt, ind);
    for (auto& node: stmt.statements) {
        dispatch(node.get(), ind + 1);
    }
}

void Context::print(FunctionDefinition const& decl, int ind) {
    printHeader(decl, ind, decl.name());
    dispatch(decl.returnTypeExpr.get(), ind + 1);
    for (auto& param: decl.parameters) {
        dispatch(param.get(), ind + 1);
    }
    dispatch(decl.body.get(), ind + 1);
}

void Context::print(StructDefinition const& decl, int ind) {
    printHeader(decl, ind, decl.name());
    dispatch(decl.body.get(), ind + 1);
}

void Context::print(VariableDeclaration const& decl, int ind) {
    printHeader(decl, ind, decl.name());
    dispatch(decl.typeExpr.get(), ind + 1);
    dispatch(decl.initExpression.get(), ind + 1);
}

void Context::print(ParameterDeclaration const& decl, int ind) {
    printHeader(decl, ind, decl.name());
    dispatch(decl.typeExpr.get(), ind + 1);
}

void Context::print(ExpressionStatement const& stmt, int ind) {
    printHeader(stmt, ind);
    dispatch(stmt.expression.get(), ind + 1);
}

void Context::print(EmptyStatement const& stmt, int ind) {
    printHeader(stmt, ind);
}

void Context::print(ReturnStatement const& stmt, int ind) {
    printHeader(stmt, ind);
    dispatch(stmt.expression.get(), ind + 1);
}

void Context::print(IfStatement const& stmt, int ind) {
    printHeader(stmt, ind);
    dispatch(stmt.condition.get(), ind + 1);
    dispatch(stmt.thenBlock.get(), ind + 1);
    dispatch(stmt.elseBlock.get(), ind + 1);
}

void Context::print(WhileStatement const& stmt, int ind) {
    printHeader(stmt, ind);
    dispatch(stmt.condition.get(), ind + 1);
    dispatch(stmt.block.get(), ind + 1);
}

void Context::print(DoWhileStatement const& stmt, int ind) {
    printHeader(stmt, ind);
    dispatch(stmt.condition.get(), ind + 1);
    dispatch(stmt.block.get(), ind + 1);
}

void Context::print(Identifier const& expr, int ind) {
    printHeader(expr, ind, expr.value());
}

void Context::print(IntegerLiteral const& expr, int ind) {
    printHeader(expr, ind, expr.value().toString());
}

void Context::print(BooleanLiteral const& expr, int ind) {
    printHeader(expr, ind, (expr.value().to<bool>() ? "true" : "false"));
}

void Context::print(FloatingPointLiteral const& expr, int ind) {
    printHeader(expr, ind, expr.value().toString());
}

void Context::print(StringLiteral const& expr, int ind) {
    printHeader(expr, ind, '"', expr.value(), '"');
}

void Context::print(UnaryPrefixExpression const& expr, int ind) {
    printHeader(expr, ind, expr.operation());
    dispatch(expr.operand.get(), ind + 1);
}

void Context::print(BinaryExpression const& expr, int ind) {
    printHeader(expr, ind, expr.operation());
    dispatch(expr.lhs.get(), ind + 1);
    dispatch(expr.rhs.get(), ind + 1);
}

void Context::print(MemberAccess const& expr, int ind) {
    printHeader(expr, ind);
    dispatch(expr.object.get(), ind + 1);
    dispatch(expr.member.get(), ind + 1);
}

void Context::print(ReferenceExpression const& expr, int ind) {
    printHeader(expr, ind);
    dispatch(expr.referred.get(), ind + 1);
}

void Context::print(UniqueExpression const& expr, int ind) {
    printHeader(expr, ind);
    dispatch(expr.initExpr.get(), ind + 1);
}

void Context::print(Conditional const& expr, int ind) {
    printHeader(expr, ind);
    dispatch(expr.condition.get(), ind + 1);
    dispatch(expr.ifExpr.get(), ind + 1);
    dispatch(expr.elseExpr.get(), ind + 1);
}

void Context::print(FunctionCall const& expr, int ind) {
    printHeader(expr, ind);
    for (auto& argument: expr.arguments) {
        dispatch(argument.get(), ind + 1);
    }
}

void Context::print(Subscript const& expr, int ind) {
    printHeader(expr, ind);
    dispatch(expr.object.get(), ind + 1);
    for (auto& argument: expr.arguments) {
        dispatch(argument.get(), ind + 1);
    }
}

void Context::print(ListExpression const& expr, int ind) {
    printHeader(expr, ind);
    for (auto* elem: expr) {
        dispatch(elem, ind + 1);
    }
}

void Context::printHeader(AbstractSyntaxTree const& node,
                          int ind,
                          auto... args) {
    str << indent(ind) << tfmt::format(tfmt::brightBlue, node.nodeType(), ": ");
    ((str << args), ...);
    str << endl;
    if (auto* expr = dyncast<Expression const*>(&node)) {
        printType(*expr, ind + 1);
    }
}
