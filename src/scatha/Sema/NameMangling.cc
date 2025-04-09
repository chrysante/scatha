#include "Sema/NameMangling.h"

#include <optional>
#include <sstream>

#include <utl/scope_guard.hpp>
#include <utl/strcat.hpp>

#include "Sema/Entity.h"
#include "Sema/QualType.h"

using namespace scatha;
using namespace sema;

namespace {

struct Impl {
    NameManglingOptions const& options;
    /// We keep track of the invocation depth of the `compute` function because
    /// we only want to prepend the global name prefix once on the top level
    int level = 0;

    auto getScopeNameAndContinue(Entity const& entity) {
        using RetType = std::pair<std::optional<std::string>, bool>;
        // clang-format off
        return SC_MATCH (entity) {
            [&](GlobalScope const&) -> RetType {
                return { std::nullopt, false };
            },
            [&](FileScope const&) -> RetType {
                if (level == 1) {
                    return { options.globalPrefix, false };
                }
                return { std::nullopt, false };
            },
            [&](NativeLibrary const& lib) -> RetType {
                if (level == 1) {
                    return { std::string(lib.name()), false };
                }
                return { std::nullopt, false };
            },
            [&](Entity const& entity) -> RetType {
                return { compute(entity), true };
            },
        }; // clang-format on
    }

    std::string computeBase(Entity const& entity) {
        std::string result = std::string(entity.name());
        Scope const* scope = entity.parent();
        /// Private symbols at file scope must be uniqued because the name may
        /// be reused
        if (auto* file = dyncast<FileScope const*>(scope);
            file && entity.isPrivate())
        {
            result = utl::strcat(result, "_F", file->index());
        }
        while (true) {
            SC_EXPECT(scope);
            auto [name, cont] = getScopeNameAndContinue(*scope);
            if (name) {
                result = utl::strcat(*name, ".", result);
            }
            if (!cont) {
                break;
            }
            scope = scope->parent();
        }
        return result;
    }

    std::string compute(Entity const& entity) {
        ++level;
        utl::scope_guard guard = [&] { --level; };
        return visit(entity,
                     [this](auto& entity) { return computeImpl(entity); });
    }

    std::string compute(QualType type) {
        std::string name = compute(*type);
        if (type.isMut()) {
            return utl::strcat("_M", name);
        }
        return name;
    }

    std::string computeImpl(Entity const&) { SC_UNREACHABLE(); }

    std::string computeImpl(VarBase const& var) { return computeBase(var); }

    std::string computeImpl(ObjectType const& type) {
        return computeBase(type);
    }

    std::string computeImpl(ArrayType const& type) {
        return utl::strcat("_A", Impl{ options }.compute(*type.elementType()));
    }

    std::string computeImpl(RawPtrType const& type) {
        return utl::strcat("_P", Impl{ options }.compute(type.base()));
    }

    std::string computeImpl(ReferenceType const& type) {
        return utl::strcat("_R", Impl{ options }.compute(type.base()));
    }

    std::string computeImpl(UniquePtrType const& type) {
        return utl::strcat("_U", Impl{ options }.compute(type.base()));
    }

    std::string computeImpl(Function const& func) {
        std::stringstream sstr;
        sstr << computeBase(func);
        for (auto* arg: func.argumentTypes()) {
            sstr << "-" << Impl{ options }.compute(*arg);
        }
        return std::move(sstr).str();
    }
};

} // namespace

std::string NameMangler::operator()(Entity const& entity) const {
    return Impl{ options() }.compute(entity);
}
