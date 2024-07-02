#include "Sema/Format.h"

#include <iostream>

#include <termfmt/termfmt.h>

#include "AST/AST.h"
#include "Sema/Entity.h"
#include "Sema/LifetimeOperation.h"

using namespace tfmt::modifiers;

using namespace scatha;
using namespace sema;

static void formatImpl(Entity const& entity, std::ostream& str) {
    if (entity.isAnonymous()) {
        str << tfmt::format(Italic, "<anonymous>");
    }
    else {
        str << entity.name();
    }
}

static void formatImpl(Temporary const& tmp, std::ostream& str) {
    str << "tmp[" << tmp.id() << "]";
}

static void formatImpl(BuiltinType const& type, std::ostream& str) {
    str << tfmt::format(Magenta | Bold, type.name());
}

static void formatImpl(RecordType const& type, std::ostream& str) {
    str << tfmt::format(Green, type.name());
}

static void formatImpl(FunctionType const& type, std::ostream& str) {
    str << tfmt::format(None, "(");
    for (bool first = true; auto* arg: type.argumentTypes()) {
        if (!first) str << tfmt::format(None, ", ");
        first = false;
        str << format(arg);
    }
    str << tfmt::format(None, ") -> ") << format(type.returnType());
}

static void formatImpl(ArrayType const& type, std::ostream& str) {
    str << tfmt::format(None, "[") << format(type.elementType());
    if (!type.isDynamic()) {
        str << tfmt::format(None, ", ", type.count());
    }
    str << tfmt::format(None, "]");
}

static void formatImpl(PointerType const& type, std::ostream& str) {
    str << tfmt::format(None, "*");
    if (isa<UniquePtrType>(type)) {
        str << tfmt::format(BrightBlue, "unique ");
    }
    str << format(type.base());
}

static void formatImpl(ReferenceType const& type, std::ostream& str) {
    str << tfmt::format(None, "&");
    str << format(type.base());
}

static void formatImpl(Function const& function, std::ostream& str) {
    str << tfmt::format(Magenta | Bold, "fn") << " ";
    if (auto* lib = dyncast<Library const*>(function.parent())) {
        str << lib->name() << ".";
    }
    str << function.name() << "(";
    for (bool first = true; auto* argType: function.argumentTypes()) {
        if (!first) {
            str << ", ";
        }
        first = false;
        str << format(argType);
    }
    str << ") -> " << format(function.returnType());
}

utl::vstreammanip<> sema::format(Entity const* entity) {
    return [=](std::ostream& str) {
        if (!entity) {
            str << tfmt::format(BrightRed, "NULL");
            return;
        }
        visit(*entity, [&](auto const& entity) { formatImpl(entity, str); });
    };
}

void sema::print(Entity const* entity, std::ostream& str) {
    str << format(entity) << "\n";
}

void sema::print(Entity const* entity) { print(entity, std::cout); }

utl::vstreammanip<> sema::format(QualType type) {
    return [=](std::ostream& str) {
        if (type.isDyn()) {
            str << tfmt::format(BrightBlue, "dyn ");
        }
        if (type.isMut()) {
            str << tfmt::format(BrightBlue, "mut ");
        }
        str << format(type.get());
    };
}

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
    return format(expr && expr->isDecorated() ? expr->type().get() : nullptr);
}
