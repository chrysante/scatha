#include "AST/Print.h"

#include <iostream>

#include <termfmt/termfmt.h>
#include <utl/strcat.hpp>
#include <utl/streammanip.hpp>

#include "AST/AST.h"
#include "Common/Base.h"
#include "Common/PrintUtil.h"
#include "Sema/Entity.h"

using namespace scatha;
using namespace ast;

void ast::printTree(AbstractSyntaxTree const& root) {
    printTree(root, std::cout);
}

static constexpr utl::streammanip typeHelper([](std::ostream& str,
                                                Indenter const* indent,
                                                Expression const* expr) {
    if (!expr->isDecorated() || !expr->type()) {
        return;
    }
    str << *indent << tfmt::format(tfmt::brightGrey, "Type: ")
        << expr->type()->name()
        << tfmt::format(tfmt::brightGrey, " [", expr->valueCategory(), "]")
        << '\n';
});

static constexpr utl::streammanip header([](std::ostream& str,
                                            Indenter* indent,
                                            AbstractSyntaxTree const* node,
                                            auto... args) {
    str << tfmt::format(tfmt::brightBlue, node->nodeType(), ": ");
    ((str << args), ...);
    str << '\n';
    if (auto* expr = dyncast<Expression const*>(node)) {
        indent->increase();
        str << typeHelper(indent, expr);
        indent->decrease();
    }
});

void ast::printTree(AbstractSyntaxTree const& root, std::ostream& str) {
    Indenter indent;
    auto print = [&](AbstractSyntaxTree const& node, auto& print) -> void {
        str << indent;
        // clang-format off
        visit(node, utl::overload{
            [&](AbstractSyntaxTree const& node) {
                str << header(&indent, &node);
            },
            [&](Literal const& expr) {
                str << header(&indent, &expr, std::visit(utl::overload{
                    [](APInt const& value)       { return value.toString(); },
                    [](APFloat const& value)     { return value.toString(); },
                    [](void*)                    { return std::string("this"); },
                    [](std::string const& value) { return utl::strcat('"', value, '"'); },
                }, expr.value()));
            },
            [&](Identifier const& id) {
                str << header(&indent, &node, id.value());
            },
            [&](UnaryPrefixExpression const& expr) {
                str << header(&indent, &node, expr.operation());
            },
            [&](BinaryExpression const& expr) {
                str << header(&indent, &node, expr.operation());
            },
            [&](Declaration const& decl) {
                str << header(&indent, &node, decl.name());
            },
            [&](LoopStatement const& loop) {
                str << header(&indent, &node, loop.kind());
            }
        }); // clang-format on
        indent.increase();
        for (auto* child: node.children()) {
            if (child) {
                print(*child, print);
            }
        }
        indent.decrease();
    };

    print(root, print);
}
