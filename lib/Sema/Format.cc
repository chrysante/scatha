#include "Sema/Format.h"

#include <iostream>

#include <termfmt/termfmt.h>

#include "AST/AST.h"
#include "Sema/Entity.h"
#include "Sema/LifetimeOperation.h"

using namespace tfmt::modifiers;

using namespace scatha;
using namespace sema;

utl::vstreammanip<> sema::format(Entity const* entity) {
    return [=](std::ostream& str) {
        if (!entity) {
            str << tfmt::format(BrightRed, "NULL");
            return;
        }
        // clang-format off
        SC_MATCH(*entity) {
            [&](Entity const& entity) {
                if (entity.isAnonymous()) {
                    str << tfmt::format(Italic, "<anonymous>");
                }
                else {
                    str << entity.name();
                }
            },
            [&](Temporary const& tmp) {
                str << "tmp[" << tmp.id() << "]";
            },
            [&](BuiltinType const& type) {
                str << tfmt::format(Magenta | Bold, type.name());
            },
            [&](StructType const& type) {
                str << tfmt::format(Green, type.name());
            },
            [&](FunctionType const& type) {
                str << tfmt::format(None, "(");
                for (bool first = true; auto* arg: type.argumentTypes()) {
                    if (!first) str << tfmt::format(None, ", ");
                    first = false;
                    str << format(arg);
                }
                str << tfmt::format(None, ") -> ") << format(type.returnType());
            },
            [&](ArrayType const& type) {
                str << tfmt::format(None, "[") << format(type.elementType());
                if (!type.isDynamic()) {
                    str << tfmt::format(None, ", ", type.count());
                }
                str << tfmt::format(None, "]");
            },
            [&](PointerType const& type) {
                str << tfmt::format(None, "*");
                if (isa<UniquePtrType>(type)) {
                    str << tfmt::format(None, "unique ");
                }
                if (type.base().isMut()) {
                    str << tfmt::format(BrightBlue, "mut ");
                }
                str << format(type.base().get());
            },
            [&](ReferenceType const& type) {
                str << tfmt::format(None, "&");
                if (type.base().isMut()) {
                    str << tfmt::format(BrightBlue, "mut ");
                }
                str << format(type.base().get());
            },
            [&](UniquePtrType const& type) {
                str << tfmt::format(None, "*unique ");
                if (type.base().isMut()) {
                    str << tfmt::format(BrightBlue, "mut ");
                }
                str << format(type.base().get());
            },
        }; // clang-format on
    };
}

void sema::print(Entity const* entity, std::ostream& str) {
    str << format(entity) << "\n";
}

void sema::print(Entity const* entity) { print(entity, std::cout); }

utl::vstreammanip<> sema::format(QualType type) { return format(type.get()); }

utl::vstreammanip<> sema::format(LifetimeOperation op) {
    return [=](std::ostream& str) {
        using enum LifetimeOperation::Kind;
        switch (op.kind()) {
        case Trivial:
            str << tfmt::format(BrightBlue, "Trivial");
            break;
        case Nontrivial:
            str << tfmt::format(Green, "Nontrivial: ") << format(op.function())
                << format(op.function()->type());
            break;
        case NontrivialInline:
            str << tfmt::format(Green, "Nontrivial (inline)");
            break;
        case Deleted:
            str << tfmt::format(Red, "Deleted");
            break;
        }
    };
}

utl::vstreammanip<> sema::formatType(ast::Expression const* expr) {
    return format(expr ? expr->type().get() : nullptr);
}
