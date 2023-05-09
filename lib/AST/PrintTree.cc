#include "AST/Print.h"

#include <iostream>

#include <termfmt/termfmt.h>
#include <utl/strcat.hpp>
#include <utl/streammanip.hpp>

#include "AST/AST.h"
#include "Common/Base.h"
#include "Common/PrintUtil.h"
#include "Sema/Analysis/ConstantExpressions.h"
#include "Sema/Analysis/Conversion.h"
#include "Sema/Entity.h"

using namespace scatha;
using namespace ast;

void ast::printTree(AbstractSyntaxTree const& root) {
    printTree(root, std::cout);
}

namespace {

enum class Level : u8 { Free, Occupied, Child, LastChild };

#define FANCY_TREE_SYMBOLS 1

#if FANCY_TREE_SYMBOLS

char const* toString(Level l) {
    switch (l) {
    case Level::Free:
        return "   ";
    case Level::Occupied:
        return "│  ";
    case Level::Child:
        return "├─ ";
    case Level::LastChild:
        return "└─ ";
    }
}

#else  //  FANCY_TREE_SYMBOLS

char const* toString(Level l) {
    switch (l) {
    case Level::Free:
        return "   ";
    case Level::Occupied:
        return "|  ";
    case Level::Leaf:
        return "|- ";
    case Level::LastLeaf:
        return "`- ";
    }
}

#endif //  FANCY_TREE_SYMBOLS

struct TreeIndenter {
    void push(Level l) {
        if (!levels.empty() && levels.back() == Level::Child) {
            levels.back() = Level::Occupied;
        }
        levels.push_back(l);
    }

    void pop() { levels.pop_back(); }

    auto put() {
        return utl::streammanip([this](std::ostream& str) {
            for (auto l: levels) {
                str << tfmt::format(tfmt::BrightGrey, toString(l));
            }
            if (!levels.empty() && levels.back() == Level::LastChild) {
                levels.back() = Level::Free;
            }
        });
    }

private:
    utl::small_vector<Level> levels;
};

} // namespace

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

static utl::vstreammanip<> typeHelper(TreeIndenter* indent,
                                      AbstractSyntaxTree const* node) {
    return [=](std::ostream& str) {
        auto* type = getType(node);
        indent->push(node->children().empty() ? Level::Free : Level::Occupied);
        if (type) {
            str << " " << tfmt::format(tfmt::BrightGrey, "Type: ")
                << type->name();
            if (auto* expr = dyncast<Expression const*>(node)) {
                str << tfmt::format(tfmt::BrightGrey,
                                    " [",
                                    expr->valueCategory(),
                                    "]");
            }
        }
        else if (auto* id = dyncast<Identifier const*>(node);
                 id && id->isDecorated())
        {
            str << tfmt::format(tfmt::BrightGrey,
                                " [",
                                id->entityCategory(),
                                "]");
        }
        indent->pop();
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
                                            TreeIndenter* indent,
                                            AbstractSyntaxTree const* node,
                                            auto... args) {
    str << indent->put() << nodeType(node);
    ((str << args), ...);
    str << typeHelper(indent, node);
    auto* expr = dyncast<Expression const*>(node);
    if (!expr || !expr->constantValue()) {
        return;
    }
    indent->push(node->children().empty() ? Level::Free : Level::Occupied);
    str << "\n" << indent->put() << tfmt::format(tfmt::BrightGrey, "Value: ");
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
    indent->pop();
});

static constexpr utl::streammanip funcDecl([](std::ostream& str,
                                              sema::Function const* func) {
    str << func->name() << "(";
    for (bool first = true; auto* type: func->argumentTypes()) {
        str << (first ? first = false, "" : ", ") << type->name();
    }
    str << ") -> " << func->returnType()->name();
});

namespace {

struct PrintCtx {
    std::ostream& str;
    TreeIndenter indent{};

    void print(AbstractSyntaxTree const& node) {
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
                auto value = tfmt::format(tfmt::Green | tfmt::Bold, id.value());
                str << header(&indent, &node, value) << '\n';
            },
            [&](UnaryPrefixExpression const& expr) {
                str << header(&indent, &node, expr.operation()) << '\n';
            },
            [&](BinaryExpression const& expr) {
                str << header(&indent, &node, expr.operation()) << '\n';
            },
            [&](Declaration const& decl) {
                auto name = tfmt::format(tfmt::Green | tfmt::Bold, decl.name());
                str << header(&indent, &node, name) << '\n';
            },
            [&](FunctionDefinition const& func) {
                str << header(&indent, &node, funcDecl(func.function())) << '\n';
            },
            [&](LoopStatement const& loop) {
                str << header(&indent, &node, loop.kind()) << '\n';
            },
            [&](Conversion const& conv) {
                str << header(&indent, &node,
                              conv.conversion()->refConversion(), ", ",
                              conv.conversion()->objectConversion()) << '\n';
            }
        }); // clang-format on
        visit(node, [&](auto& node) { printChildren(node); });
    }

    void printChildren(AbstractSyntaxTree const& node) {
        printChildrenImpl(node.children());
    }

    void printChildren(FunctionDefinition const& node) {
        indent.push(Level::Child);
        for (auto* param: node.parameters()) {
            print(*param);
        }
        indent.pop();
        indent.push(Level::LastChild);
        print(*node.body());
        indent.pop();
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
            indent.push(index != children.size() - 1 ? Level::Child :
                                                       Level::LastChild);
            print(*child);
            indent.pop();
        }
    }
};

} // namespace

void ast::printTree(AbstractSyntaxTree const& root, std::ostream& str) {
    PrintCtx ctx{ str };
    ctx.print(root);
}
