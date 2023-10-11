#include "Sema/Format.h"

#include <ostream>

#include <termfmt/termfmt.h>

#include "AST/AST.h"
#include "Sema/Entity.h"

using namespace tfmt::modifiers;

using namespace scatha;
using namespace sema;

utl::vstreammanip<> sema::format(Type const* type) {
    return [=](std::ostream& str) {
        if (!type) {
            str << "<null-type>";
            return;
        }
        // clang-format off
        SC_MATCH(*type) {
            [&](BuiltinType const& type) {
                str << tfmt::format(Magenta | Bold, type.name());
            },
            [&](StructType const& type) {
                str << tfmt::format(Green, type.name());
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

utl::vstreammanip<> sema::format(QualType type) { return format(type.get()); }

utl::vstreammanip<> sema::formatType(ast::Expression const* expr) {
    return format(expr ? expr->type().get() : nullptr);
}
