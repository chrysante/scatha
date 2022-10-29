#ifndef SCATHA_SEMA_FUNCTION_H_
#define SCATHA_SEMA_FUNCTION_H_

#include "Sema/EntityBase.h"
#include "Sema/FunctionSignature.h"

namespace scatha::sema {

class SCATHA(API) Function: public Scope {
public:
    explicit Function(std::string name, SymbolID functionID, SymbolID overloadSetID, Scope* parentScope):
        Scope(ScopeKind::Function, std::move(name), functionID, parentScope), _overloadSetID(overloadSetID) {}

    TypeID typeID() const { return signature().typeID(); }
    SymbolID overloadSetID() const { return _overloadSetID; }

    FunctionSignature const& signature() const { return _sig; }

private:
    friend class SymbolTable; /// To set \p _sig
    FunctionSignature _sig;
    SymbolID _overloadSetID;
};

namespace internal {

struct FunctionArgumentsHash {
    struct is_transparent;
    size_t operator()(Function const* f) const { return f->signature().argumentHash(); }
    size_t operator()(std::span<TypeID const> const& args) const { return FunctionSignature::hashArguments(args); }
};

struct FunctionArgumentsEqual {
    struct is_transparent;

    using Args = std::span<TypeID const>;

    bool operator()(Function const* a, Function const* b) const {
        return a->signature().argumentHash() == b->signature().argumentHash();
    }
    bool operator()(Function const* a, Args b) const {
        return a->signature().argumentHash() == FunctionSignature::hashArguments(b);
    }
    bool operator()(Args a, Function const* b) const { return (*this)(b, a); }
    bool operator()(Args a, Args b) const {
        return FunctionSignature::hashArguments(a) == FunctionSignature::hashArguments(b);
    }
};

} // namespace internal

} // namespace scatha::sema

#endif // SCATHA_SEMA_FUNCTION_H_
