#include "Sema/Print.h"

#include <iostream>

#include <termfmt/termfmt.h>
#include <utl/streammanip.hpp>

#include "Common/TreeFormatter.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;

void sema::print(SymbolTable const& symbolTable) {
    print(symbolTable, std::cout);
}

namespace {

struct Context {
    Context(std::ostream& str, SymbolTable const& sym): str(str), sym(sym) {}

    void print(Entity const& entity);

    utl::vstreammanip<> name(Entity const& entity);

    utl::vstreammanip<> nameImpl(Entity const* entity) {
        return [=](std::ostream& str) { str << entity->name(); };
    }

    utl::vstreammanip<> nameImpl(Function const* function) {
        return [=](std::ostream& str) {
            str << "(";
            for (bool first = true; auto* type: function->argumentTypes()) {
                str << (first ? first = false, "" : ", ") << type->name();
            }
            str << ") -> " << function->returnType()->name();
        };
    }

    utl::vstreammanip<> nameImpl(GlobalScope const*) {
        return [](std::ostream& str) { str << "Top level"; };
    }

    auto filterBuiltins() const {
        return ranges::views::filter([=](Entity const* entity) {
            return printBuiltins || !entity->isBuiltin();
        });
    }

    std::ostream& str;
    SymbolTable const& sym;
    TreeFormatter formatter;
    bool printBuiltins = false;
};

} // namespace

void sema::print(SymbolTable const& symbolTable, std::ostream& ostream) {
    Context ctx(ostream, symbolTable);
    ctx.print(symbolTable.globalScope());
}

void Context::print(Entity const& entity) {
    str << formatter.beginLine() << name(entity) << " "
        << tfmt::format(tfmt::BrightGrey, "[", entity.entityType(), "]")
        << "\n";
    auto children = [&] {
        using SetType = utl::hashset<Entity const*>;
        if (auto* scope = dyncast<Scope const*>(&entity)) {
            auto children = scope->entities() | filterBuiltins() |
                            ranges::to<utl::hashset<Entity const*>>;
            for (auto* entity: scope->children() | filterBuiltins()) {
                if (isa<Function>(entity)) {
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

utl::vstreammanip<> Context::name(Entity const& entity) {
    return visit(entity, [this](auto& entity) { return nameImpl(&entity); });
}
