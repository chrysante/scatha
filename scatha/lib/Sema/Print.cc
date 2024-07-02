#include "Sema/Print.h"

#include <iostream>

#include <range/v3/view.hpp>
#include <termfmt/termfmt.h>
#include <utl/streammanip.hpp>

#include "Common/TreeFormatter.h"
#include "Sema/Entity.h"
#include "Sema/Format.h"
#include "Sema/LifetimeMetadata.h"
#include "Sema/QualType.h"
#include "Sema/SymbolTable.h"
#include "Sema/VTable.h"

using namespace scatha;
using namespace sema;
using namespace ranges::views;
using namespace tfmt::modifiers;

void sema::print(SymbolTable const& symbolTable) {
    print(symbolTable, std::cout);
}

namespace {

struct PrintContext {
    PrintContext(std::ostream& str, SymbolTable const& sym,
                 PrintOptions const& options):
        str(str), sym(sym), options(options) {}

    void print(Entity const& entity);

    void print(LifetimeMetadata const& md);

    void print(VTable const& vtable);

    utl::vstreammanip<> type(Type const* type) {
        return [=](std::ostream& str) { str << format(type); };
    }

    utl::vstreammanip<> name(Entity const& entity);

    utl::vstreammanip<> nameImpl(Entity const& entity) {
        return [&](std::ostream& str) { str << entity.name(); };
    }

    utl::vstreammanip<> nameImpl(Object const& object) {
        return [&, this](std::ostream& str) {
            str << object.name() << ": " << type(object.type());
        };
    }

    utl::vstreammanip<> nameImpl(Type const& ty) { return type(&ty); }

    utl::vstreammanip<> nameImpl(Function const& function) {
        return [&, this](std::ostream& str) {
            str << function.name() << (function.type() ? "" : " ")
                << type(function.type());
        };
    }

    utl::vstreammanip<> nameImpl(GlobalScope const*) {
        return [](std::ostream& str) { str << "Global Scope"; };
    }

    utl::hashset<Entity const*> getChildren(Entity const& entity) const;

    std::ostream& str;
    SymbolTable const& sym;
    TreeFormatter formatter;
    PrintOptions const& options;
};

} // namespace

void sema::print(SymbolTable const& symbolTable, std::ostream& ostream,
                 PrintOptions const& options) {
    PrintContext ctx(ostream, symbolTable, options);
    ctx.print(symbolTable.globalScope());
}

static utl::streammanip constexpr formatSize([](std::ostream& str,
                                                size_t size) {
    if (size == InvalidSize) {
        str << "Invalid";
    }
    else {
        str << size;
    }
});

void PrintContext::print(Entity const& entity) {
    str << formatter.beginLine();
    if (!entity.isVisible()) {
        str << tfmt::format(BrightGrey, "[hidden]") << " ";
    }
    str << name(entity) << " ";
    tfmt::formatScope(BrightGrey, str, [&] {
        str << "[";
        if (entity.hasAccessControl()) {
            str << entity.accessControl() << " ";
        }
        if (auto* fn = dyncast<Function const*>(&entity);
            fn && fn->isGenerated())
        {
            str << "generated"
                << " ";
        }
        str << entity.entityType() << "]";
    });
    str << "\n";
    auto children = getChildren(entity);
    if (auto* type = dyncast<ObjectType const*>(&entity)) {
        formatter.push(children.empty() ? Level::LastChild : Level::Child);
        str << formatter.beginLine() << tfmt::format(Italic, "Size: ")
            << formatSize(type->size()) << "\n";
        str << formatter.beginLine() << tfmt::format(Italic, "Align: ")
            << formatSize(type->align()) << "\n";
        if (type->hasLifetimeMetadata()) {
            print(type->lifetimeMetadata());
        }
        formatter.pop();
    }
    for (auto [index, child]: children | ranges::views::enumerate) {
        formatter.push(index != children.size() - 1 ? Level::Child :
                                                      Level::LastChild);
        print(*child);
        formatter.pop();
    }
    if (auto* record = dyncast<RecordType const*>(&entity);
        record && record->vtable())
    {
        print(*record->vtable());
    }
}

void PrintContext::print(LifetimeMetadata const& md) {
    str << formatter.beginLine() << "Lifetime: \n";
    for (auto [index, op]: md.operations() | ranges::views::enumerate) {
        bool last = index == md.operations().size() - 1;
        formatter.push(last ? Level::LastChild : Level::Child);
        str << formatter.beginLine()
            << tfmt::format(Italic, SMFKind(index), ": ") << format(op) << "\n";
        formatter.pop();
    }
}

void PrintContext::print(VTable const& vtable) {
    str << formatter.beginLine() << "VTable for " << format(vtable.type())
        << ": \n";
    auto inherited = vtable.sortedInheritedVTables();
    for (auto [index, base]: inherited | ranges::views::enumerate) {
        bool last = index == inherited.size() - 1 && vtable.layout().empty();
        formatter.push(last ? Level::LastChild : Level::Child);
        print(*base);
        formatter.pop();
    }
    for (auto [index, F]: vtable.layout() | ranges::views::enumerate) {
        bool last = index == vtable.layout().size() - 1;
        formatter.push(last ? Level::LastChild : Level::Child);
        str << formatter.beginLine() << format(F) << "\n";
        formatter.pop();
    }
}

utl::vstreammanip<> PrintContext::name(Entity const& entity) {
    return visit(entity, [this](auto& entity) { return nameImpl(entity); });
}

static bool isBuiltinExt(Entity const& entity) {
    // clang-format off
    return SC_MATCH (entity) {
        [](ArrayType const& type) {
            return type.isBuiltin() ||
                   (type.elementType() && isBuiltinExt(*type.elementType()));
        },
        [](PointerType const& type) {
            return type.isBuiltin() ||
                   (type.base() && isBuiltinExt(*type.base()));
        },
        [](ReferenceType const& type) {
            return type.isBuiltin() ||
                   (type.base() && isBuiltinExt(*type.base()));
        },
        [](Entity const& entity) {
            return entity.isBuiltin();
        }
    }; // clang-format on
}

utl::hashset<Entity const*> PrintContext::getChildren(
    Entity const& entity) const {
    utl::hashset<Entity const*> children;
    auto* scope = dyncast<Scope const*>(&entity);
    if (!scope) {
        return children;
    }
    for (auto* entity: scope->entities()) {
        if (!options.printBuiltins && isBuiltinExt(*entity)) {
            continue;
        }
        children.insert(entity);
    }
    for (auto* entity: scope->children()) {
        if (isa<Function>(scope)) {
            continue;
        }
        /// We don't want to print local entities like function variables or
        /// temporaries, so we skip functions here
        if (!options.printBuiltins && isBuiltinExt(*entity)) {
            continue;
        }
        children.insert(entity);
    }
    return children;
}
