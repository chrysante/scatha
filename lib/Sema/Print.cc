#include "Sema/Print.h"

#include <iostream>

#include <utl/hashset.hpp>

#include "Basic/PrintUtil.h"

namespace scatha::sema {

void printSymbolTable(SymbolTable const& sym) {
    printSymbolTable(sym, std::cout);
}

class internal::ScopePrinter {
public:
    SymbolTable const& sym;
    void printScope(Scope const& scope, std::ostream& str, int ind);
};

static constexpr auto endl = '\n';

static Indenter indent(int level) {
    return Indenter(level, 2);
}

void printSymbolTable(SymbolTable const& sym, std::ostream& str) {
    internal::ScopePrinter p{ sym };
    p.printScope(sym.globalScope(), str, 0);
}

void internal::ScopePrinter::printScope(Scope const& scope,
                                        std::ostream& str,
                                        int ind) {
    struct PrintData {
        std::string_view name;
        EntityBase const* entity;
        SymbolID id;
        SymbolCategory cat;
    };
    utl::hashset<SymbolID> printedScopes;
    utl::vector<PrintData> data;
    for (auto&& [name, id]: scope._symbols) {
        if (id.category() == SymbolCategory::Variable) {
            data.push_back(
                { name, &sym.getVariable(id), id, SymbolCategory::Variable });
            continue;
        }
        if (id.category() == SymbolCategory::ObjectType) {
            if (sym.getObjectType(id).isBuiltin()) {
                printedScopes.insert(id);
                continue;
            }
            data.push_back({ name,
                             &sym.getObjectType(id),
                             id,
                             SymbolCategory::ObjectType });
            continue;
        }
        SC_ASSERT(id.category() == SymbolCategory::OverloadSet, "What else?");
        for (auto const& function: sym.getOverloadSet(id)) {
            // clang-format off
            data.push_back({
                name,
                &sym.getFunction(function->symbolID()),
                function->symbolID(),
                SymbolCategory::Function
            });
            // clang-format on
        }
    }

    for (auto [name, entity, id, cat]: data) {
        str << indent(ind) << cat << " " << makeQualName(*entity);
        if (cat == SymbolCategory::Function) {
            auto& fn = sym.getFunction(id);
            str << "(";
            for (bool first = true; auto id: fn.signature().argumentTypeIDs()) {
                if (!first) {
                    str << ", ";
                }
                first = false;
                str << (id ? makeQualName(sym.getObjectType(id)) :
                             "<invalid-type>");
            }
            str << ") -> "
                << makeQualName(
                       sym.getObjectType(fn.signature().returnTypeID()));
        }
        else if (cat == SymbolCategory::ObjectType) {
            auto& type     = sym.getObjectType(id);
            auto printSize = [&str](size_t s) {
                if (s == invalidSize) {
                    str << "invalid";
                }
                else {
                    str << s;
                }
            };
            str << " [size: ";
            printSize(type.size());
            str << ", align: ";
            printSize(type.align());
            str << "]";
        }
        else if (cat == SymbolCategory::Variable) {
            auto& var = sym.getVariable(id);
            str << ": "
                << (var.typeID() ?
                        makeQualName(sym.getObjectType(var.typeID())) :
                        "<invalid-type>");
        }
        str << endl;
        auto const itr = scope._children.find(id);
        if (itr == scope._children.end()) {
            continue;
        }
        auto const [_, insertSuccess] = printedScopes.insert(id);
        SC_ASSERT(insertSuccess, "");
        auto const& childScope = itr->second;
        printScope(*childScope, str, ind + 1);
    }
    for (auto&& [id, childScope]: scope._children) {
        if (printedScopes.contains(id)) {
            continue;
        }
        str << indent(ind) << "<anonymous-scope-" << id.rawValue() << ">"
            << endl;
        printScope(*childScope, str, ind + 1);
    }
}

std::string makeQualName(EntityBase const& ent) {
    std::string result = std::string(ent.name());
    Scope const* s     = ent.parent();
    while (s->kind() != ScopeKind::Global) {
        result = std::string(s->name()) + "." + result;
        s      = s->parent();
    }
    return result;
}

} // namespace scatha::sema
