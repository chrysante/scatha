#include "Sema/NameMangling.h"

#include <optional>
#include <sstream>

#include <utl/strcat.hpp>

#include "Sema/Entity.h"
#include "Sema/QualType.h"

using namespace scatha;
using namespace sema;

namespace {

struct Impl {
    NameManglingOptions const& options;

    auto getScopeNameAndContinue(Entity const& entity) {
        using RetType = std::pair<std::optional<std::string>, bool>;
        // clang-format off
        return SC_MATCH (entity) {
            [&](GlobalScope const&) -> RetType {
                return { std::nullopt, false };
            },
            [&](FileScope const&) -> RetType {
                return { options.globalPrefix, false };
            },
            [&](NativeLibrary const& lib) -> RetType {
                return { lib.libName(), false };
            },
            [&](Entity const& entity) -> RetType {
                return { std::string(entity.name()), true };
            },
        }; // clang-format on
    }

    std::string computeBase(Entity const& entity) {
        std::string result = std::string(entity.name());
        Scope const* scope = entity.parent();
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
        return visit(entity,
                     [this](auto& entity) { return computeImpl(entity); });
    }

    std::string compute(QualType type) {
        std::string name = compute(*type);
        if (type.isMut()) {
            return utl::strcat("mut-", name);
        }
        return name;
    }

    std::string computeImpl(Entity const&) { SC_UNREACHABLE(); }

    std::string computeImpl(ObjectType const& type) {
        return computeBase(type);
    }

    std::string computeImpl(ArrayType const& type) {
        return utl::strcat("[", compute(type.elementType()), "]");
    }

    std::string computeImpl(RawPtrType const& type) {
        return utl::strcat("*", compute(type.base()));
    }

    std::string computeImpl(ReferenceType const& type) {
        return utl::strcat("&", compute(type.base()));
    }

    std::string computeImpl(UniquePtrType const& type) {
        return utl::strcat("*unique-", compute(type.base()));
    }

    std::string computeImpl(Function const& func) {
        std::stringstream sstr;
        sstr << computeBase(func);
        for (auto* arg: func.signature().argumentTypes()) {
            sstr << "-" << compute(*arg);
        }
        return std::move(sstr).str();
    }
};

} // namespace

std::string NameMangler::operator()(Entity const& entity) const {
    return Impl{ options() }.compute(entity);
}
