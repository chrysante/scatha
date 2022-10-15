#include "AST/PrintSource.h"

#include <iostream>

#include <utl/utility.hpp>

#include "AST/AST.h"
#include "AST/Visit.h"
#include "Basic/Basic.h"
#include "Basic/PrintUtil.h"

namespace scatha::ast {

void printSource(AbstractSyntaxTree const* root) {
    printSource(root, std::cout);
}

namespace {
struct Context {

    std::ostream& str;
    EndlIndenter endl{ 4 };

    void print(AbstractSyntaxTree const& node) {
        visit(
            node,
            utl::visitor{
                [&](TranslationUnit const& tu) {
            for (auto& decl : tu.declarations) {
                print(*decl);
                str << endl << endl;
            }
                },
                [&](Block const& block) {
            str << "{";
            if (block.statements.empty()) {
                str << endl;
            }
            for (auto [i, s] : utl::enumerate(block.statements)) {
                if (i == 0) {
                    endl.increase();
                }
                str << endl;
                print(*s);
                if (i == block.statements.size() - 1) {
                    endl.decrease();
                }
            }
            str << endl << "}";
            },
            [&](FunctionDefinition const& fn) {
            str << "fn " << fn.name() << "(";
            for (bool first = true; auto const& param : fn.parameters) {
                str << (first ? ((void)(first = false), "") : ", ") << param->name() << ": ";
                print(*param->typeExpr);
            }
            str << ") -> ";
            print(*fn.returnTypeExpr);
            str << " ";
            print(*fn.body);
            },
            [&](StructDefinition const& s) {
            str << "struct " << s.name() << " ";
            print(*s.body);
        },
            [&](VariableDeclaration const& var) {
            str << (var.isConstant ? "let" : "var") << " " << var.name() << ": ";
            if (var.typeExpr) {
                print(*var.typeExpr);
            } else {
                str << "<deduce-type>";
            }
            if (var.initExpression) {
                str << " = ";
                print(*var.initExpression);
            }
            str << ";";
            },
            [&](ExpressionStatement const& es) {
            SC_ASSERT(es.expression != nullptr,
                      "Expression statement without an expression is "
                      "invalid");
            print(*es.expression);
            str << ";";
            },
            [&](ReturnStatement const& rs) {
            str << "return";
            if (!rs.expression) {
                return;
            }
            str << " ";
            print(*rs.expression);
            str << ";";
            },
            [&](IfStatement const& is) {
            str << "if ";
            print(*is.condition);
            str << " ";
            print(*is.ifBlock);
            if (!is.elseBlock) {
                return;
            }
            str << endl << "else ";
            print(*is.elseBlock);
            },
            [&](WhileStatement const& ws) {
            str << "while ";
            print(*ws.condition);
            str << " ";
            print(*ws.block);
            }, [&](Identifier const& i) { str << i.value(); }, [&](IntegerLiteral const& l) { str << l.value; },
            [&](BooleanLiteral const& l) { str << l.value; }, [&](FloatingPointLiteral const& l) { str << l.value; },
            [&](StringLiteral const& l) { str << l.value; },
            [&](UnaryPrefixExpression const& u) {
            str << u.op << '(';
            print(*u.operand);
            str << ')';
            },
            [&](BinaryExpression const& b) {
            str << '(';
            print(*b.lhs);
            str << ' ' << b.op << ' ';
            print(*b.rhs);
            str << ')';
            },
            [&](MemberAccess const& ma) {
            print(*ma.object);
            str << ".";
            print(*ma.member);
            },
            [&](Conditional const& c) {
            str << "((";
            print(*c.condition);
            str << ") ? (";
            print(*c.ifExpr);
            str << ") : (";
            print(*c.elseExpr);
            str << "))";
            },
            [&](FunctionCall const& f) {
            print(*f.object);
            str << "(";
            for (bool first = true; auto& arg : f.arguments) {
                if (!first) {
                    str << ", ";
                } else {
                    first = false;
                }
                print(*arg);
            }
            str << ")";
            },
            [&](Subscript const& f) {
            print(*f.object);
            str << "(";
            for (bool first = true; auto& arg : f.arguments) {
                if (!first) {
                    str << ", ";
                } else {
                    first = false;
                }
                print(*arg);
            }
            str << ")";
            },
            });
    }
};
} // namespace

void printSource(AbstractSyntaxTree const* root, std::ostream& str) {
    Context ctx{ str, {} };
    ctx.print(*root);
}

} // namespace scatha::ast
