#include "Sema/Print.h"

#include <iostream>

#include <range/v3/view.hpp>
#include <termfmt/termfmt.h>
#include <utl/streammanip.hpp>

#include "Common/TreeFormatter.h"
#include "Sema/Entity.h"
#include "Sema/Format.h"
#include "Sema/QualType.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;
using namespace ranges::views;

void sema::print(SymbolTable const& symbolTable) {
    print(symbolTable, std::cout);
}

namespace {

struct PrintContext {
    PrintContext(std::ostream& str, SymbolTable const& sym):
        str(str), sym(sym) {}

    void print(Entity const& entity);

    utl::vstreammanip<> type(Type const* type) {
        return [=](std::ostream& str) {
            if (type) {
                str << format(type);
            }
            else {
                str << "NULL";
            }
        };
    }

    utl::vstreammanip<> name(Entity const& entity);

    utl::vstreammanip<> nameImpl(Entity const* entity) {
        return [=](std::ostream& str) { str << entity->name(); };
    }

    utl::vstreammanip<> nameImpl(Object const* object) {
        return [=, this](std::ostream& str) {
            str << object->name() << ": " << type(object->type());
        };
    }

    utl::vstreammanip<> nameImpl(Type const* ty) { return type(ty); }

    utl::vstreammanip<> nameImpl(Function const* function) {
        return [=, this](std::ostream& str) {
            str << function->name() << "(";
            for (bool first = true; auto* argType: function->argumentTypes()) {
                str << (first ? first = false, "" : ", ") << type(argType);
            }
            str << ") -> " << type(function->returnType());
        };
    }

    utl::vstreammanip<> nameImpl(GlobalScope const*) {
        return [](std::ostream& str) { str << "Global Scope"; };
    }

    utl::vstreammanip<> nameImpl(Library const* lib) {
        return [=](std::ostream& str) { str << lib->libName(); };
    }

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
    str << formatter.beginLine() << name(entity) << " ";
    tfmt::format(tfmt::BrightGrey, str, [&] {
        str << "[";
        if (entity.hasAccessControl()) {
            str << entity.accessControl() << " ";
        }
        str << entity.entityType() << "]";
    });
    str << "\n";
    auto children = [&] {
        using SetType = utl::hashset<Entity const*>;
        if (auto* scope = dyncast<Scope const*>(&entity)) {
            auto children = scope->entities() | filter([](auto* entity) {
                return !entity->isBuiltin();
            }) | ranges::to<utl::hashset<Entity const*>>;
            for (auto* entity: scope->children()) {
                if (isa<Function>(entity) || entity->isBuiltin()) {
                    continue;
                }
                children.insert(entity);
            }
            return children;
        }
        if (auto* os = dyncast<OverloadSet const*>(&entity)) {
            return *os | ranges::to<SetType>;
        }
        return SetType{};
    }();
    for (auto [index, child]: children | ranges::views::enumerate) {
        formatter.push(index != children.size() - 1 ? Level::Child :
                                                      Level::LastChild);
        print(*child);
        formatter.pop();
    }
}

utl::vstreammanip<> PrintContext::name(Entity const& entity) {
    return visit(entity, [this](auto& entity) { return nameImpl(&entity); });
}
