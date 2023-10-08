#include "AST/Print.h"

#include <iostream>

#include <termfmt/termfmt.h>
#include <utl/strcat.hpp>
#include <utl/streammanip.hpp>

#include "AST/AST.h"
#include "Common/Base.h"
#include "Common/EscapeSequence.h"
#include "Common/PrintUtil.h"
#include "Common/Ranges.h"
#include "Common/TreeFormatter.h"
#include "Sema/Analysis/ConstantExpressions.h"
#include "Sema/Analysis/Conversion.h"
#include "Sema/Entity.h"

using namespace scatha;
using namespace ast;
using namespace tfmt::modifiers;

void ast::printTree(ASTNode const& root) { printTree(root, std::cout); }

static utl::vstreammanip<> formatType(sema::Type const* type) {
    return [=](std::ostream& str) {
        str << " " << tfmt::format(BrightGrey, "Type: ");
        if (!type) {
            str << tfmt::format(Red, "NULL");
            return;
        }
        str << type->name();
    };
}

static constexpr utl::streammanip nodeType([](std::ostream& str,
                                              ASTNode const* node) {
    tfmt::FormatGuard common(Italic);
    // clang-format off
    visit(*node, utl::overload{
        [&](ASTNode const& node) {
            str << node.nodeType();
        },
        [&](Statement const& node) {
            str << tfmt::format(BrightBlue, node.nodeType());
        }
    }); // clang-format on
    str << ": ";
});

static constexpr utl::streammanip nodeHeader([](std::ostream& str,
                                                TreeFormatter* formatter,
                                                ASTNode const* node,
                                                auto... args) {
    str << formatter->beginLine() << nodeType(node);
    ((str << args), ...);
    if (!node->isDecorated()) {
        return;
    }
    if (auto* expr = dyncast<Expression const*>(node); expr && expr->isValue())
    {
        str << formatType(expr->type().get()) << " "
            << tfmt::format(BrightGrey, expr->valueCategory());
    }
    else if (auto* decl = dyncast<VarDeclBase const*>(node)) {
        str << formatType(decl->type());
    }
    auto* expr = dyncast<Expression const*>(node);
    if (!expr || !expr->constantValue()) {
        return;
    }
    formatter->push(node->children().empty() ? Level::Free : Level::Occupied);
    str << "\n"
        << formatter->beginLine() << tfmt::format(BrightGrey, "Value: ");
    // clang-format off
    SC_MATCH (*expr->constantValue()) {
        [&](sema::IntValue const& node) {
            auto value = node.value();
            str << (node.isSigned() ? value.signedToString() :
                                              value.toString());
        },
        [&](sema::FloatValue const& node) {
            str << node.value().toString();
        }
    }; // clang-format on
    formatter->pop();
});

static constexpr utl::streammanip funcDecl([](std::ostream& str,
                                              sema::Function const* func) {
    tfmt::FormatGuard bold(Bold);
    str << func->name() << "(";
    for (bool first = true; auto type: func->argumentTypes()) {
        str << (first ? first = false, "" : ", ");
        str << (type ? type->name() : "NULL");
    }
    str << ") -> " << func->returnType()->name();
});

static constexpr utl::streammanip formatLit([](std::ostream& str,
                                               ast::Literal const* lit) {
    switch (lit->kind()) {
    case LiteralKind::Integer: {
        auto* type = cast<sema::IntType const*>(lit->type().get());
        auto value = lit->value<APInt>();
        if (!type) {
            str << value.toString();
            break;
        }
        if (type->isSigned()) {
            str << value.signedToString();
            break;
        }
        if (ucmp(value, APInt(0x10000, value.bitwidth())) >= 0) {
            str << value.toString(16);
            break;
        }
        str << value.toString();
        break;
    }
    case LiteralKind::Boolean:
        str << (lit->value<APInt>().test(1) ? "true" : "false");
        break;
    case LiteralKind::FloatingPoint:
        str << lit->value<APFloat>().toString();
        break;
    case LiteralKind::Null:
        str << "null";
        break;
    case LiteralKind::This:
        str << "this";
        break;
    case LiteralKind::String:
        str << '"';
        printWithEscapeSeqs(str, lit->value<std::string>());
        str << '"';
        break;
    case LiteralKind::Char:
        str << '\'';
        char charVal = lit->value<APInt>().to<char>();
        if (auto raw = fromEscapeSequence(charVal)) {
            str << '\\' << *raw;
        }
        else {
            str << charVal;
        }
        str << '\'';
        break;
    }
});

namespace {

struct PrintCtx {
    std::ostream& str;
    TreeFormatter formatter;

    explicit PrintCtx(std::ostream& str): str(str) {}

    void print(ASTNode const& node) {
        if (!node.isDecorated()) {
            str << nodeHeader(&formatter, &node) << '\n';
            goto end;
        }

        // clang-format off
        visit(node, utl::overload{
            [&](ASTNode const& node) {
                str << nodeHeader(&formatter, &node) << '\n';
            },
            [&](Literal const& lit) {
                str << nodeHeader(&formatter, &node, formatLit(&lit)) << '\n';
            },
            [&](Identifier const& id) {
                auto value = tfmt::format(Green | Bold, id.value());
                str << nodeHeader(&formatter, &node, value) << '\n';
            },
            [&](UnaryExpression const& expr) {
                str << nodeHeader(&formatter, &node, expr.operation()) << '\n';
            },
            [&](BinaryExpression const& expr) {
                str << nodeHeader(&formatter, &node, expr.operation()) << '\n';
            },
            [&](Declaration const& decl) {
                auto name = tfmt::format(Green | Bold, decl.name());
                str << nodeHeader(&formatter, &node, name) << '\n';
            },
            [&](FunctionDefinition const& func) {
                str << nodeHeader(&formatter, &node, funcDecl(func.function())) << '\n';
            },
            [&](LoopStatement const& loop) {
                str << nodeHeader(&formatter, &node, loop.kind()) << '\n';
            },
            [&](Conversion const& conv) {
                str << nodeHeader(&formatter, &node,
                              conv.conversion()->valueCatConversion(), ", ",
                              conv.conversion()->objectConversion()) << '\n';
            }
        }); // clang-format on

end:
        visit(node, [&](auto& node) { printChildren(node); });
    }

    void printChildren(ASTNode const& node) {
        printChildrenImpl(node.children());
    }

    void printChildren(FunctionDefinition const& node) {
        formatter.push(Level::Child);
        for (auto* param: node.parameters()) {
            print(*param);
        }
        formatter.pop();
        formatter.push(Level::LastChild);
        print(*node.body());
        formatter.pop();
    }

    void printChildren(VariableDeclaration const& node) {
        printChildrenImpl(node.children() | ranges::views::drop(1));
    }

    void printChildren(ParameterDeclaration const& node) {}

    void printChildrenImpl(auto&& c) {
        auto children = c | ranges::views::filter([](auto* child) {
                            return child != nullptr;
                        }) |
                        ToSmallVector<>;
        for (auto [index, child]: children | ranges::views::enumerate) {
            formatter.push(index != children.size() - 1 ? Level::Child :
                                                          Level::LastChild);
            print(*child);
            formatter.pop();
        }
    }
};

} // namespace

void ast::printTree(ASTNode const& root, std::ostream& str) {
    PrintCtx ctx(str);
    ctx.print(root);
}
