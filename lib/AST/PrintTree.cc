#include "AST/Print.h"

#include <iostream>

#include <termfmt/termfmt.h>
#include <utl/strcat.hpp>
#include <utl/streammanip.hpp>

#include "AST/AST.h"
#include "Common/Base.h"
#include "Common/EscapeSequence.h"
#include "Common/PrintUtil.h"
#include "Common/TreeFormatter.h"
#include "Sema/Analysis/ConstantExpressions.h"
#include "Sema/Analysis/Conversion.h"
#include "Sema/Entity.h"

using namespace scatha;
using namespace ast;

void ast::printTree(AbstractSyntaxTree const& root) {
    printTree(root, std::cout);
}

static sema::QualType const* getType(AbstractSyntaxTree const* node) {
    if (!node->isDecorated()) {
        return nullptr;
    }
    // clang-format off
    return visit(*node, utl::overload{
        [](AbstractSyntaxTree const& node) { return nullptr; },
        [](Expression const& expr) { return expr.type(); },
        [](VariableDeclaration const& decl) { return decl.type(); },
        [](ParameterDeclaration const& decl) { return decl.type(); },
    }); // clang-format on
}

static utl::vstreammanip<> typeHelper(AbstractSyntaxTree const* node) {
    return [=](std::ostream& str) {
        auto* type = getType(node);
        if (type) {
            str << " " << tfmt::format(tfmt::BrightGrey, "Type: ")
                << type->name();
            if (auto* expr = dyncast<Expression const*>(node)) {
                str << tfmt::format(tfmt::BrightGrey,
                                    " [",
                                    expr->valueCategory(),
                                    "]");
            }
            return;
        }
        if (auto* id = dyncast<Identifier const*>(node);
            id && id->isDecorated())
        {
            str << tfmt::format(tfmt::BrightGrey,
                                " [",
                                id->entityCategory(),
                                "]");
            return;
        }
    };
}

static constexpr utl::streammanip nodeType([](std::ostream& str,
                                              AbstractSyntaxTree const* node) {
    tfmt::FormatGuard common(tfmt::Italic);
    // clang-format off
    visit(*node, utl::overload{
        [&](AbstractSyntaxTree const& node) {
            str << node.nodeType();
        },
        [&](Statement const& node) {
            str << tfmt::format(tfmt::BrightBlue, node.nodeType());
        }
    }); // clang-format on
    str << ": ";
});

static constexpr utl::streammanip header([](std::ostream& str,
                                            TreeFormatter* formatter,
                                            AbstractSyntaxTree const* node,
                                            auto... args) {
    str << formatter->beginLine() << nodeType(node);
    ((str << args), ...);
    str << typeHelper(node);
    auto* expr = dyncast<Expression const*>(node);
    if (!expr || !expr->constantValue()) {
        return;
    }
    formatter->push(node->children().empty() ? Level::Free : Level::Occupied);
    str << "\n"
        << formatter->beginLine() << tfmt::format(tfmt::BrightGrey, "Value: ");
    // clang-format off
    visit(*expr->constantValue(), utl::overload{
        [&](sema::IntValue const& node) {
            auto value = node.value();
            str << (node.isSigned() ? value.signedToString() :
                                              value.toString());
        },
        [&](sema::FloatValue const& node) {
            str << node.value().toString();
        }
    }); // clang-format on
    formatter->pop();
});

static constexpr utl::streammanip funcDecl([](std::ostream& str,
                                              sema::Function const* func) {
    str << func->name() << "(";
    for (bool first = true; auto* type: func->argumentTypes()) {
        str << (first ? first = false, "" : ", ");
        str << (type ? type->name() : "NULL");
    }
    str << ") -> " << func->returnType()->name();
});

static constexpr utl::streammanip formatLit([](std::ostream& str,
                                               ast::Literal const* lit) {
    switch (lit->kind()) {
    case LiteralKind::Integer: {
        auto* type = cast<sema::IntType const*>(lit->type()->base());
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

    void print(AbstractSyntaxTree const& node) {
        if (!node.isDecorated()) {
            str << header(&formatter, &node) << '\n';
            goto end;
        }

        // clang-format off
        visit(node, utl::overload{
            [&](AbstractSyntaxTree const& node) {
                str << header(&formatter, &node) << '\n';
            },
            [&](Literal const& lit) {
                str << header(&formatter, &node, formatLit(&lit)) << '\n';
            },
            [&](Identifier const& id) {
                auto value = tfmt::format(tfmt::Green | tfmt::Bold, id.value());
                str << header(&formatter, &node, value) << '\n';
            },
            [&](UnaryExpression const& expr) {
                str << header(&formatter, &node, expr.operation()) << '\n';
            },
            [&](BinaryExpression const& expr) {
                str << header(&formatter, &node, expr.operation()) << '\n';
            },
            [&](Declaration const& decl) {
                auto name = tfmt::format(tfmt::Green | tfmt::Bold, decl.name());
                str << header(&formatter, &node, name) << '\n';
            },
            [&](FunctionDefinition const& func) {
                str << header(&formatter, &node, funcDecl(func.function())) << '\n';
            },
            [&](LoopStatement const& loop) {
                str << header(&formatter, &node, loop.kind()) << '\n';
            },
            [&](Conversion const& conv) {
                str << header(&formatter, &node,
                              conv.conversion()->refConversion(), ", ",
                              conv.conversion()->objectConversion()) << '\n';
            }
        }); // clang-format on

end:
        visit(node, [&](auto& node) { printChildren(node); });
    }

    void printChildren(AbstractSyntaxTree const& node) {
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
        auto children =
            c | ranges::views::filter([](auto* child) {
                return child != nullptr;
            }) |
            ranges::to<utl::small_vector<AbstractSyntaxTree const*>>;
        for (auto [index, child]: children | ranges::views::enumerate) {
            formatter.push(index != children.size() - 1 ? Level::Child :
                                                          Level::LastChild);
            print(*child);
            formatter.pop();
        }
    }
};

} // namespace

void ast::printTree(AbstractSyntaxTree const& root, std::ostream& str) {
    PrintCtx ctx(str);
    ctx.print(root);
}
