#ifndef SCATHA_IRGEN_LOWERINGCONTEXT_H_
#define SCATHA_IRGEN_LOWERINGCONTEXT_H_

#include <deque>
#include <string>

#include <utl/hashtable.hpp>
#include <utl/strcat.hpp>

#include "AST/Fwd.h"
#include "IR/Fwd.h"
#include "IRGen/IRGen.h"
#include "Sema/Fwd.h"

namespace scatha::irgen {

class TypeMap;
class GlobalMap;
struct GlobalVarMetadata;

struct ThunkKey {
    sema::Function const* concreteFunction;
    sema::RecordType const* vtableType;

    bool operator==(ThunkKey const&) const = default;
};

} // namespace scatha::irgen

template <>
struct std::hash<scatha::irgen::ThunkKey> {
    std::size_t operator()(scatha::irgen::ThunkKey key) const {
        return utl::hash_combine(key.concreteFunction, key.vtableType);
    }
};

namespace scatha::irgen {

/// Helper class to find a library symbol by mangled name or abort
class ImportMap {
public:
    template <typename T>
    void insert(T* t) {
        auto& map = getMap<std::remove_const_t<T>>();
        map.insert({ std::string(t->name()), t });
    }

    template <typename TargetType>
    TargetType* get(std::string_view name) const {
        auto* result = tryGet<TargetType>(name);
        SC_RELASSERT(result, utl::strcat("Failed to find symbol '", name,
                                         "' in library")
                                 .c_str());
        return result;
    }

    template <typename TargetType>
    TargetType* get(auto const&... args) const
        requires(sizeof...(args) > 1)
    {
        return get<TargetType>(utl::strcat(args...));
    }

    template <typename TargetType>
    TargetType* tryGet(std::string_view name) const {
        auto& map = getMap<TargetType>();
        auto itr = map.find(name);
        return itr != map.end() ? cast<TargetType*>(itr->second) : nullptr;
    }

    template <typename TargetType>
    TargetType* tryGet(auto const&... args) const
        requires(sizeof...(args) > 1)
    {
        return tryGet<TargetType>(utl::strcat(args...));
    }

private:
    template <typename TargetType>
    auto& getMap() {
        auto& map = std::as_const(*this).getMap<TargetType>();
        return const_cast<std::remove_cvref_t<decltype(map)>&>(map);
    }

    template <typename TargetType>
    auto const& getMap() const {
        if constexpr (std::derived_from<TargetType, ir::StructType>) {
            return types;
        }
        else if constexpr (std::derived_from<TargetType, ir::Global>) {
            return objects;
        }
        else {
            static_assert(!std::same_as<TargetType, TargetType>,
                          "Invalid argument type");
        }
    }

    utl::hashmap<std::string, ir::StructType*> types;
    utl::hashmap<std::string, ir::Global*> objects;
};

/// Objects used during the entire lowering process
struct LoweringContext {
    ir::Context& ctx;
    ir::Module& mod;
    sema::SymbolTable const& symbolTable;
    TypeMap& typeMap;
    GlobalMap& globalMap;
    std::deque<sema::Function const*>& declQueue;
    /// All functions that have beed added to the decl queue
    utl::hashset<sema::Function const*>& lowered;
    /// To avoid generating the same thunk twice we cache them here
    utl::hashmap<ThunkKey, ir::Function*>& thunkMap;
    ImportMap& importMap;
    Config const& config;
};

} // namespace scatha::irgen

#endif // SCATHA_IRGEN_LOWERINGCONTEXT_H_
