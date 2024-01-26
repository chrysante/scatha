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
#include "Sema/CleanupStack.h"
#include "Sema/Entity.h"
#include "Sema/Format.h"

using namespace scatha;
using namespace ast;
using namespace tfmt::modifiers;
using namespace ranges::views;

void ast::print(ASTNode const& root) { print(root, std::cout); }

static utl::vstreammanip<> formatType(sema::Type const* type) {
    return [=](std::ostream& str) {
        str << tfmt::format(BrightGrey, "Type: ") << sema::format(type);
    };
}

static utl::vstreammanip<> formatID(auto... args) {
    return
        [=](std::ostream& str) { str << tfmt::format(Green | Bold, args...); };
}

static utl::vstreammanip<> formatObject(sema::Object const* obj) {
    return [=](std::ostream& str) {
        str << tfmt::format(BrightGrey, obj->entityType()) << " ";
        SC_MATCH (*obj) {
            [&](sema::Variable const& var) { str << formatID(var.name()); },
                [&](sema::Property const& prop) { str << prop.kind(); },
                [&](sema::Temporary const& tmp) {
                str << "[" << tmp.id() << "]";
            },
        };
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
        },
        [&](Expression const& node) {
            str << tfmt::format(Yellow, node.nodeType());
        }
    }); // clang-format on
    str << ": ";
});

static void printConstantValue(std::ostream& str,
                               TreeFormatter& formatter,
                               sema::Value const& value) {
    str << "\n" << formatter.beginLine() << tfmt::format(BrightGrey, "Value: ");
    // clang-format off
    SC_MATCH (value) {
        [&](sema::IntValue const& node) {
            auto value = node.value();
            str << (node.isSigned() ? value.signedToString() :
                                              value.toString());
        },
        [&](sema::FloatValue const& node) {
            str << node.value().toString();
        }
    }; // clang-format on
}

static void printCleanupStack(std::ostream& str,
                              TreeFormatter& formatter,
                              sema::CleanupStack const& cleanupStack) {
    if (cleanupStack.empty()) {
        return;
    }
    str << formatter.beginLine() << tfmt::format(Bold | BrightGrey, "Cleanups:")
        << "\n";
    for (auto [index, op]: cleanupStack | enumerate) {
        formatter.push(index != cleanupStack.size() - 1 ? Level::Child :
                                                          Level::LastChild);
        str << formatter.beginLine() << formatObject(op.object) << "\n";
        formatter.pop();
    }
}

static constexpr utl::streammanip nodeHeader([](std::ostream& str,
                                                TreeFormatter& formatter,
                                                ASTNode const& node,
                                                auto... args) {
    str << formatter.beginLine() << nodeType(&node);
    ((str << args), ...);
    if (!node.isDecorated()) {
        return;
    }
    if (sizeof...(args) > 0) {
        str << ", ";
    }
    if (auto* expr = dyncast<Expression const*>(&node); expr && expr->isValue())
    {
        if (auto* tmp = dyncast<sema::Temporary const*>(expr->entity())) {
            str << formatObject(tmp) << ", ";
        }
        str << formatType(expr->type().get()) << " "
            << tfmt::format(BrightGrey, expr->valueCategory());
    }
    else if (auto* decl = dyncast<VarDeclBase const*>(&node)) {
        str << formatType(decl->type());
    }
    /// Print additional information
    formatter.push(node.children().empty() ? Level::Free : Level::Occupied);
    if (auto* expr = dyncast<Expression const*>(&node);
        expr && expr->constantValue())
    {
        printConstantValue(str, formatter, *expr->constantValue());
    }
    formatter.pop();
});

static constexpr utl::streammanip funcDecl([](std::ostream& str,
                                              sema::Function const* func) {
    str << formatID(func->name()) << ": " << sema::format(func->type());
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
            str << nodeHeader(formatter, node) << '\n';
        }
        else {
            // clang-format off
            SC_MATCH (node) {
                [&](ASTNode const& node) {
                    str << nodeHeader(formatter, node) << '\n';
                },
                [&](TranslationUnit const&) {},
                [&](ast::SourceFile const& file) {
                    str << nodeHeader(formatter, file, file.name()) << '\n';
                },
                [&](Literal const& lit) {
                    str << nodeHeader(formatter, node, formatLit(&lit)) 
                        << '\n';
                },
                [&](Identifier const& id) {
                    str << nodeHeader(formatter, node, formatID(id.value()))
                        << '\n';
                },
                [&](UnaryExpression const& expr) {
                    str << nodeHeader(formatter, node, expr.operation()) 
                        << '\n';
                },
                [&](BinaryExpression const& expr) {
                    str << nodeHeader(formatter, node, expr.operation()) 
                        << '\n';
                },
                [&](Declaration const& decl) {
                    str << nodeHeader(formatter, node, formatID(decl.name()))
                        << '\n';
                },
                [&](FunctionDefinition const& func) {
                    str << nodeHeader(formatter, node, 
                                      funcDecl(func.function())) << '\n';
                },
                [&](LoopStatement const& loop) {
                    str << nodeHeader(formatter, node, loop.kind()) << '\n';
                },
                [&](std::derived_from<ConvExprBase> auto const& expr) {
                    str << nodeHeader(formatter, node,
                                      expr.conversion()) << "\n";
                },
                [&](NontrivConstructExpr const& expr) {
                    str << nodeHeader(formatter, expr) << "\n";
                    formatter.push(expr.children().empty() ? Level::Free :
                                                             Level::Occupied);
                    str << formatter.beginLine()
                        << tfmt::format(BrightGrey, "Selected constructor: ")
                        << funcDecl(expr.constructor()) << "\n";
                    formatter.pop();
                },
            }; // clang-format on
        }
        if (auto* stmt = dyncast<Statement const*>(&node)) {
            formatter.push(node.children().empty() ? Level::Free :
                                                     Level::Occupied);
            printCleanupStack(str, formatter, stmt->cleanupStack());
            formatter.pop();
        }
        visit(node, [&](auto& node) { printChildren(node); });
    }

    void printChildren(ASTNode const& node) {
        printChildrenImpl(node.children());
    }

    void printChildren(TranslationUnit const& node) {
        for (auto* child: node.children() | NonNull) {
            print(*child);
        }
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
        printChildrenImpl(node.children() | drop(1));
    }

    void printChildren(ParameterDeclaration const&) {}

    void printChildrenImpl(auto&& c) {
        auto children = c |
                        filter([](auto* child) { return child != nullptr; }) |
                        ToSmallVector<>;
        for (auto [index, child]: children | enumerate) {
            formatter.push(index != children.size() - 1 ? Level::Child :
                                                          Level::LastChild);
            print(*child);
            formatter.pop();
        }
    }
};

} // namespace

void ast::print(ASTNode const& root, std::ostream& str) {
    PrintCtx ctx(str);
    ctx.print(root);
}
