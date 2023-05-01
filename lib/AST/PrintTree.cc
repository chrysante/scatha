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

static constexpr utl::streammanip typeHelper(
    [](std::ostream& str, Indenter* indent, AbstractSyntaxTree const* node) {
    auto* type = getType(node);
    if (!type) {
        return;
    }
    indent->increase();
    str << '\n'
        << *indent << tfmt::format(tfmt::brightGrey, "Type: ") << type->name();
    if (auto* expr = dyncast<Expression const*>(node)) {
        str << tfmt::format(tfmt::brightGrey, " [", expr->valueCategory(), "]");
    }
    indent->decrease();
});

static constexpr utl::streammanip header([](std::ostream& str,
                                            Indenter* indent,
                                            AbstractSyntaxTree const* node,
                                            auto... args) {
    str << *indent << tfmt::format(tfmt::brightBlue, node->nodeType(), ": ");
    ((str << args), ...);
    str << typeHelper(indent, node);
});

void ast::printTree(AbstractSyntaxTree const& root, std::ostream& str) {
    Indenter indent(3);
    auto print = [&](AbstractSyntaxTree const& node, auto& print) -> void {
        // clang-format off
        visit(node, utl::overload{
            [&](AbstractSyntaxTree const& node) {
                str << header(&indent, &node) << '\n';
            },
            [&](Literal const& expr) {
                str << header(&indent, &expr, std::visit(utl::overload{
                    [](APInt const& value)       { return value.toString(); },
                    [](APFloat const& value)     { return value.toString(); },
                    [](void*)                    { return std::string("this"); },
                    [](std::string const& value) { return utl::strcat('"', value, '"'); },
                }, expr.value())) << '\n';
            },
            [&](Identifier const& id) {
                str << header(&indent, &node, id.value()) << '\n';
            },
            [&](UnaryPrefixExpression const& expr) {
                str << header(&indent, &node, expr.operation()) << '\n';
            },
            [&](BinaryExpression const& expr) {
                str << header(&indent, &node, expr.operation()) << '\n';
            },
            [&](Declaration const& decl) {
                str << header(&indent, &node, decl.name()) << '\n';
            },
            [&](LoopStatement const& loop) {
                str << header(&indent, &node, loop.kind()) << '\n';
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
