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

using namespace scatha;
using namespace sema;
using namespace ranges::views;
using namespace tfmt::modifiers;

void sema::print(SymbolTable const& symbolTable) {
    print(symbolTable, std::cout);
}

namespace {

struct PrintContext {
    PrintContext(std::ostream& str, SymbolTable const& sym):
        str(str), sym(sym) {}

    void print(Entity const& entity);

    void print(LifetimeMetadata const& md);

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
    bool printBuiltins = false;
};

} // namespace

void sema::print(SymbolTable const& symbolTable, std::ostream& ostream) {
    PrintContext ctx(ostream, symbolTable);
    ctx.print(symbolTable.globalScope());
}

void PrintContext::print(Entity const& entity) {
    str << formatter.beginLine();
    if (!entity.isVisible()) {
        str << tfmt::format(BrightGrey, "[hidden]") << " ";
    }
    str << name(entity) << " ";
    tfmt::format(BrightGrey, str, [&] {
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
    if (auto* type = dyncast<ObjectType const*>(&entity);
        type && type->hasLifetimeMetadata())
    {
        formatter.push(children.empty() ? Level::LastChild : Level::Child);
        print(type->lifetimeMetadata());
        formatter.pop();
    }
    for (auto [index, child]: children | ranges::views::enumerate) {
        formatter.push(index != children.size() - 1 ? Level::Child :
                                                      Level::LastChild);
        print(*child);
        formatter.pop();
    }
}

static constexpr utl::streammanip formatLifetimeOp = [](std::ostream& str,
                                                        LifetimeOperation op) {
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

void PrintContext::print(LifetimeMetadata const& md) {
    str << formatter.beginLine() << "Lifetime: \n";
    for (auto [index, op]: md.operations() | ranges::views::enumerate) {
        formatter.push(index != md.operations().size() - 1 ? Level::Child :
                                                             Level::LastChild);
        str << formatter.beginLine()
            << tfmt::format(Italic, SMFKind(index), ": ")
            << formatLifetimeOp(op) << "\n";
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
    using SetType = utl::hashset<Entity const*>;
    auto* scope = dyncast<Scope const*>(&entity);
    if (!scope) {
        return SetType{};
    }
    auto children = scope->entities() | filter([](auto* entity) {
        return !isBuiltinExt(*entity);
    }) | ranges::to<SetType>;
    for (auto* entity: scope->children()) {
        /// We don't want to print local entities like function variables or
        /// temporaries, so we skip functions here
        if (isa<Function>(entity) || isBuiltinExt(*entity)) {
            continue;
        }
        children.insert(entity);
    }
    return children;
}
