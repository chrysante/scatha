#include "AST/Print.h"

#include <iostream>

#include <termfmt/termfmt.h>
#include <utl/streammanip.hpp>

#include "AST/AST.h"
#include "Common/Base.h"
#include "Common/PrintUtil.h"
#include "Sema/QualType.h"

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
    void print(AbstractSyntaxTree const&, int ind) {
        str << indent(ind) << "<unknown>" << endl;
    }

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

    static constexpr utl::streammanip nodeType =
        [](std::ostream& str, auto... args) {
        str << tfmt::format(tfmt::brightBlue, "<", args..., ">");
    };

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
    str << indent(ind) << nodeType("translation-unit") << endl;
    for (auto& decl: tu.declarations) {
        dispatch(decl.get(), ind + 1);
    }
}

void Context::print(CompoundStatement const& block, int ind) {
    str << indent(ind) << nodeType("block") << endl;
    for (auto& node: block.statements) {
        dispatch(node.get(), ind + 1);
    }
}

void Context::print(FunctionDefinition const& fnDef, int ind) {
    str << indent(ind) << nodeType("function-definition") << " " << fnDef.name()
        << endl;
    dispatch(fnDef.returnTypeExpr.get(), ind + 1);
    for (auto& param: fnDef.parameters) {
        dispatch(param.get(), ind + 1);
    }
    dispatch(fnDef.body.get(), ind + 1);
}

void Context::print(StructDefinition const& structDef, int ind) {
    str << indent(ind) << nodeType("struct-definition") << " "
        << structDef.name();
    str << endl;
    dispatch(structDef.body.get(), ind + 1);
}

void Context::print(VariableDeclaration const& varDecl, int ind) {
    str << indent(ind) << nodeType("variable-declaration") << " "
        << varDecl.name();
    str << " " << endl;
    dispatch(varDecl.typeExpr.get(), ind + 1);
    dispatch(varDecl.initExpression.get(), ind + 1);
}

void Context::print(ParameterDeclaration const& paramDecl, int ind) {
    str << indent(ind) << nodeType("parameter-declaration") << " "
        << paramDecl.name();
    str << " " << endl;
    dispatch(paramDecl.typeExpr.get(), ind + 1);
}

void Context::print(ExpressionStatement const& exprStatement, int ind) {
    str << indent(ind) << nodeType("expression-statement") << endl;
    dispatch(exprStatement.expression.get(), ind + 1);
}

void Context::print(EmptyStatement const&, int ind) {
    str << indent(ind) << nodeType("empty-statement") << endl;
}

void Context::print(ReturnStatement const& returnStatement, int ind) {
    str << indent(ind) << nodeType("return-statement") << endl;
    dispatch(returnStatement.expression.get(), ind + 1);
}

void Context::print(IfStatement const& ifStatement, int ind) {
    str << indent(ind) << nodeType("if-statement") << endl;
    dispatch(ifStatement.condition.get(), ind + 1);
    dispatch(ifStatement.thenBlock.get(), ind + 1);
    dispatch(ifStatement.elseBlock.get(), ind + 1);
}

void Context::print(WhileStatement const& whileStatement, int ind) {
    str << indent(ind) << nodeType("while-statement") << endl;
    dispatch(whileStatement.condition.get(), ind + 1);
    dispatch(whileStatement.block.get(), ind + 1);
}

void Context::print(DoWhileStatement const& doWhileStatement, int ind) {
    str << indent(ind) << nodeType("do-while-statement") << endl;
    dispatch(doWhileStatement.condition.get(), ind + 1);
    dispatch(doWhileStatement.block.get(), ind + 1);
}

void Context::print(Identifier const& identifier, int ind) {
    str << indent(ind) << nodeType("identifier") << " " << identifier.value()
        << endl;
    printType(identifier, ind + 1);
}

void Context::print(IntegerLiteral const& intLiteral, int ind) {
    str << indent(ind) << nodeType("integer-literal") << " "
        << intLiteral.value().toString() << endl;
    printType(intLiteral, ind + 1);
}

void Context::print(BooleanLiteral const& boolLiteral, int ind) {
    str << indent(ind) << nodeType("boolean-literal") << " "
        << (boolLiteral.value().to<bool>() ? "true" : "false") << endl;
    printType(boolLiteral, ind + 1);
}

void Context::print(FloatingPointLiteral const& floatLiteral, int ind) {
    str << indent(ind) << nodeType("float-literal") << " "
        << floatLiteral.value().toString() << endl;
    printType(floatLiteral, ind + 1);
}

void Context::print(StringLiteral const& stringLiteral, int ind) {
    str << indent(ind) << nodeType("string-literal") << " " << '"'
        << stringLiteral.value() << '"' << endl;
    printType(stringLiteral, ind + 1);
}

void Context::print(UnaryPrefixExpression const& unaryPrefExpr, int ind) {
    str << indent(ind) << nodeType("unary-prefix-expression") << " "
        << unaryPrefExpr.operation() << endl;
    printType(unaryPrefExpr, ind + 1);
    dispatch(unaryPrefExpr.operand.get(), ind + 1);
}

void Context::print(BinaryExpression const& binExpr, int ind) {
    str << indent(ind) << nodeType("binary-expression") << " "
        << binExpr.operation() << endl;
    printType(binExpr, ind + 1);
    dispatch(binExpr.lhs.get(), ind + 1);
    dispatch(binExpr.rhs.get(), ind + 1);
}

void Context::print(MemberAccess const& memberAccess, int ind) {
    str << indent(ind) << nodeType("member-access") << endl;
    printType(memberAccess, ind + 1);
    dispatch(memberAccess.object.get(), ind + 1);
    dispatch(memberAccess.member.get(), ind + 1);
}

void Context::print(ReferenceExpression const& ref, int ind) {
    str << indent(ind) << nodeType("reference") << endl;
    printType(ref, ind + 1);
    dispatch(ref.referred.get(), ind + 1);
}

void Context::print(UniqueExpression const& ref, int ind) {
    str << indent(ind) << nodeType("unique") << endl;
    printType(ref, ind + 1);
    dispatch(ref.initExpr.get(), ind + 1);
}

void Context::print(Conditional const& conditional, int ind) {
    str << indent(ind) << nodeType("conditional") << endl;
    printType(conditional, ind + 1);
    dispatch(conditional.condition.get(), ind + 1);
    dispatch(conditional.ifExpr.get(), ind + 1);
    dispatch(conditional.elseExpr.get(), ind + 1);
}

void Context::print(FunctionCall const& functionCall, int ind) {
    str << indent(ind) << nodeType("function-call") << endl;
    dispatch(functionCall.object.get(), ind + 1);
    for (auto& argument: functionCall.arguments) {
        dispatch(argument.get(), ind + 1);
    }
}

void Context::print(Subscript const& subscript, int ind) {
    str << indent(ind) << nodeType("subscript") << endl;
    printType(subscript, ind + 1);
    dispatch(subscript.object.get(), ind + 1);
    for (auto& argument: subscript.arguments) {
        dispatch(argument.get(), ind + 1);
    }
}

void Context::print(ListExpression const& list, int ind) {
    str << indent(ind) << nodeType("list") << endl;
    printType(list, ind + 1);
    for (auto* elem: list) {
        dispatch(elem, ind + 1);
    }
}
