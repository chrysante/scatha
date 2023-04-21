#include "AST/Print.h"

#include <iomanip>
#include <iostream>

#include <range/v3/view.hpp>
#include <utl/common.hpp>

#include "AST/AST.h"
#include "Common/Base.h"
#include "Common/PrintUtil.h"

using namespace scatha;
using namespace ast;

namespace {

struct Context {
    void dispatch(AbstractSyntaxTree const&);

    void print(TranslationUnit const&);
    void print(CompoundStatement const&);
    void print(FunctionDefinition const&);
    void print(StructDefinition const&);
    void print(VariableDeclaration const&);
    void print(ParameterDeclaration const&);
    void print(ExpressionStatement const&);
    void print(EmptyStatement const&);
    void print(ReturnStatement const&);
    void print(IfStatement const&);
    void print(WhileStatement const&);
    void print(DoWhileStatement const&);
    void print(Identifier const&);
    void print(IntegerLiteral const&);
    void print(BooleanLiteral const&);
    void print(FloatingPointLiteral const&);
    void print(StringLiteral const&);
    void print(UnaryPrefixExpression const&);
    void print(BinaryExpression const&);
    void print(MemberAccess const&);
    void print(Conditional const&);
    void print(FunctionCall const&);
    void print(Subscript const&);
    void print(AbstractSyntaxTree const&) { SC_UNREACHABLE(); }

    std::ostream& str;
    EndlIndenter endl{};
};

} // namespace

void ast::printSource(AbstractSyntaxTree const& root) {
    printSource(root, std::cout);
}

void ast::printSource(AbstractSyntaxTree const& root, std::ostream& str) {
    Context ctx{ .str = str, .endl = EndlIndenter(4) };
    ctx.dispatch(root);
}

void Context::dispatch(AbstractSyntaxTree const& node) {
    visit(node, [&](auto const& node) { print(node); });
}

void Context::print(TranslationUnit const& tu) {
    for (auto& decl: tu.declarations) {
        dispatch(*decl);
        str << endl << endl;
    }
}

void Context::print(CompoundStatement const& block) {
    str << "{";
    if (block.statements.empty()) {
        str << endl;
    }
    for (auto [i, s]: ranges::views::enumerate(block.statements)) {
        if (i == 0) {
            endl.increase();
        }
        str << endl;
        dispatch(*s);
        if (i == block.statements.size() - 1) {
            endl.decrease();
        }
    }
    str << endl << "}";
}

void Context::print(FunctionDefinition const& fn) {
    str << "fn ";
    dispatch(*fn.nameIdentifier);
    str << "(";
    for (bool first = true; auto const& param: fn.parameters) {
        str << (first ? ((void)(first = false), "") : ", ");
        dispatch(*param->nameIdentifier);
        str << ": ";
        dispatch(*param->typeExpr);
    }
    str << ")";
    if (fn.returnTypeExpr) {
        str << " -> ";
        dispatch(*fn.returnTypeExpr);
    }
    str << " ";
    dispatch(*fn.body);
}

void Context::print(StructDefinition const& s) {
    str << "struct ";
    dispatch(*s.nameIdentifier);
    str << " ";
    dispatch(*s.body);
}

void Context::print(VariableDeclaration const& var) {
    str << (var.isConstant ? "let" : "var");
    dispatch(*var.nameIdentifier);
    str << ": ";
    if (var.typeExpr) {
        dispatch(*var.typeExpr);
    }
    else {
        str << "<deduce-type>";
    }
    if (var.initExpression) {
        str << " = ";
        dispatch(*var.initExpression);
    }
    str << ";";
}

void Context::print(ParameterDeclaration const& param) {
    dispatch(*param.nameIdentifier);
    str << ": ";
    if (param.typeExpr) {
        dispatch(*param.typeExpr);
    }
    else {
        str << "<invalid-type>";
    }
}

void Context::print(ExpressionStatement const& es) {
    if (es.expression) {
        dispatch(*es.expression);
    }
    else {
        str << "<invalid-expression>";
    }
    str << ";";
}

void Context::print(EmptyStatement const&) { str << ";"; }

void Context::print(ReturnStatement const& rs) {
    str << "return";
    if (!rs.expression) {
        return;
    }
    str << " ";
    dispatch(*rs.expression);
    str << ";";
}

void Context::print(IfStatement const& is) {
    str << "if ";
    dispatch(*is.condition);
    str << " ";
    dispatch(*is.thenBlock);
    if (!is.elseBlock) {
        return;
    }
    str << endl << "else ";
    dispatch(*is.elseBlock);
}

void Context::print(WhileStatement const& ws) {
    str << "while ";
    dispatch(*ws.condition);
    str << " ";
    dispatch(*ws.block);
}

void Context::print(DoWhileStatement const& ws) {
    str << "do ";
    dispatch(*ws.block);
    str << " ";
    dispatch(*ws.condition);
    str << ";";
}

void Context::print(Identifier const& i) { str << i.value(); }

void Context::print(IntegerLiteral const& l) {
    str << l.value().signedToString();
}

void Context::print(BooleanLiteral const& l) {
    str << (l.value().to<bool>() ? "true" : "false");
}

void Context::print(FloatingPointLiteral const& l) {
    UTL_STORE_STREAM_STATE(str);
    str << l.value().toString();
}

void Context::print(StringLiteral const& l) { str << '"' << l.value() << '"'; }

void Context::print(UnaryPrefixExpression const& u) {
    str << u.operation() << '(';
    dispatch(*u.operand);
    str << ')';
}

void Context::print(BinaryExpression const& b) {
    str << '(';
    dispatch(*b.lhs);
    str << ' ' << b.operation() << ' ';
    dispatch(*b.rhs);
    str << ')';
}

void Context::print(MemberAccess const& ma) {
    dispatch(*ma.object);
    str << ".";
    dispatch(*ma.member);
}

void Context::print(Conditional const& c) {
    str << "((";
    dispatch(*c.condition);
    str << ") ? (";
    dispatch(*c.ifExpr);
    str << ") : (";
    dispatch(*c.elseExpr);
    str << "))";
}

void Context::print(FunctionCall const& f) {
    dispatch(*f.object);
    str << "(";
    for (bool first = true; auto& arg: f.arguments) {
        if (!first) {
            str << ", ";
        }
        else {
            first = false;
        }
        dispatch(*arg);
    }
    str << ")";
}

void Context::print(Subscript const& f) {
    dispatch(*f.object);
    str << "(";
    for (bool first = true; auto& arg: f.arguments) {
        if (!first) {
            str << ", ";
        }
        else {
            first = false;
        }
        dispatch(*arg);
    }
    str << ")";
}
