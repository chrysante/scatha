#ifndef SCATHA_IRGEN_LOWERINGCONTEXT_H_
#define SCATHA_IRGEN_LOWERINGCONTEXT_H_

#include <deque>

#include <utl/hashtable.hpp>

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
    Config const& config;
};

} // namespace scatha::irgen

#endif // SCATHA_IRGEN_LOWERINGCONTEXT_H_
