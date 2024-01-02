#include "Sema/NameMangling.h"

#include <sstream>

#include <utl/strcat.hpp>

#include "Sema/Entity.h"
#include "Sema/QualType.h"

using namespace scatha;
using namespace sema;

namespace {

struct Impl {
    NameManglingOptions const& options;

    std::string computeBase(Entity const& entity) {
        std::string result = std::string(entity.name());
        Scope const* scope = entity.parent();
        SC_EXPECT(scope);
        while (true) {
            if (scope->kind() == ScopeKind::Global) {
                break;
            }
            result = utl::strcat(scope->name(), ".", result);
            scope = scope->parent();
            SC_EXPECT(scope);
        }
        if (options.globalPrefix) {
            return utl::strcat(*options.globalPrefix, ".", result);
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
