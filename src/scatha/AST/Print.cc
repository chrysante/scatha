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

static utl::vstreammanip<> formatType(auto type) {
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
        // clang-format off
        SC_MATCH (*obj) {
            [&](sema::Variable const& var) { str << formatID(var.name()); },
            [&](sema::BaseClassObject const& base) {
                str << formatID(base.name());
            },
            [&](sema::Property const& prop) { str << prop.kind(); },
            [&](sema::Temporary const& tmp) {
                str << "[" << tmp.id() << "]";
            },
        }; // clang-format on
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

static void printConstantValue(std::ostream& str, TreeFormatter& formatter,
                               sema::Value const& value) {
    str << "\n" << formatter.beginLine() << tfmt::format(BrightGrey, "Value: ");
    // clang-format off
    SC_MATCH (value) {
        [&](sema::IntValue const& node) {
            auto val = node.value();
            str << (node.isSigned() ? val.signedToString() : val.toString());
        },
        [&](sema::FloatValue const& node) {
            str << node.value().toString();
        },
        [&](sema::PointerValue const& value) {
            SC_ASSERT(value.isNull(), "");
            str << tfmt::format(Magenta | Bold, "null");
        },
    }; // clang-format on
}

static void printCleanupStack(std::ostream& str, TreeFormatter& formatter,
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
        str << formatType(expr->type()) << " "
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
    auto printString = [&](std::string_view begin, std::string_view end) {
        str << begin;
        printWithEscapeSeqs(str, lit->value<std::string>());
        str << end;
    };
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
        printString("\"", "\"");
        break;
    case LiteralKind::FStringBegin:
        printString("\"", "\\(");
        break;
    case LiteralKind::FStringContinue:
        printString(")", "\\(");
        break;
    case LiteralKind::FStringEnd:
        printString(")", "\"");
        break;
    case LiteralKind::Char:
        printString("\'", "\'");
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
            str << nodeHeader(formatter, node);
        }
        else {
            // clang-format off
            SC_MATCH (node) {
                [&](ASTNode const& node) {
                    str << nodeHeader(formatter, node);
                },
                [&](TranslationUnit const&) {},
                [&](ast::SourceFile const& file) {
                    str << nodeHeader(formatter, file, file.name());
                },
                [&](Literal const& lit) {
                    str << nodeHeader(formatter, node, formatLit(&lit));
                },
                [&](Identifier const& id) {
                    str << nodeHeader(formatter, node, formatID(id.value()));
                },
                [&](UnaryExpression const& expr) {
                    str << nodeHeader(formatter, node, expr.operation());
                },
                [&](BinaryExpression const& expr) {
                    str << nodeHeader(formatter, node, expr.operation());
                },
                [&](FunctionCall const& expr) {
                    str << nodeHeader(formatter, expr, expr.callBinding());
                },
                [&](Declaration const& decl) {
                    str << nodeHeader(formatter, node, formatID(decl.name()));
                },
                [&](FunctionDefinition const& func) {
                    str << nodeHeader(formatter, node, 
                                      funcDecl(func.function()));
                },
                [&](LoopStatement const& loop) {
                    str << nodeHeader(formatter, node, loop.kind());
                },
                [&](std::derived_from<ConvExprBase> auto const& expr) {
                    str << nodeHeader(formatter, node,
                                      expr.conversion());
                },
                [&](MoveExpr const& expr) {
                    str << nodeHeader(formatter, expr);
                    if (auto op = expr.operation()) {
                        str << ", " << sema::format(*op);
                    }
                },
                [&](NontrivConstructExpr const& expr) {
                    str << nodeHeader(formatter, expr) << "\n";
                    formatter.push(expr.children().empty() ? Level::Free :
                                                             Level::Occupied);
                    str << formatter.beginLine()
                        << tfmt::format(BrightGrey, "Selected constructor: ")
                        << funcDecl(expr.constructor());
                    formatter.pop();
                },
            }; // clang-format on
        }
        if (auto* stmt = dyncast<Statement const*>(&node);
            stmt && !stmt->reachable())
        {
            str << " " << tfmt::format(Red, "Unreachable");
        }
        str << "\n";
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
        if (node.body()) {
            print(*node.body());
        }
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
