#ifndef SCATHA_SEMA_FUNCTION_H_
#define SCATHA_SEMA_FUNCTION_H_

#include "Sema/EntityBase.h"
#include "Sema/FunctionSignature.h"

namespace scatha::sema {

class SCATHA(API) Function: public Scope {
  public:
    struct ArgumentHash {
        struct is_transparent;
        size_t operator()(Function const &f) const { return f.signature().argumentHash(); }
        size_t operator()(std::span<TypeID const> const &args) const { return FunctionSignature::hashArguments(args); }
    };

    struct ArgumentEqual {
        struct is_transparent;

        using Args = std::span<TypeID const>;

        bool operator()(Function const &a, Function const &b) const {
            return a.signature().argumentHash() == b.signature().argumentHash();
        }
        bool operator()(Function const &a, Args b) const {
            return a.signature().argumentHash() == FunctionSignature::hashArguments(b);
        }
        bool operator()(Args a, Function const &b) const {
            return FunctionSignature::hashArguments(a) == b.signature().argumentHash();
        }
        bool operator()(Args a, Args b) const {
            return FunctionSignature::hashArguments(a) == FunctionSignature::hashArguments(b);
        }
    };

  public:
    explicit Function(std::string name, FunctionSignature signature, SymbolID symbolID, Scope *parentScope):
        Scope(ScopeKind::Function, std::move(name), symbolID, parentScope), _sig(signature) {}

    TypeID                   typeID() const { return signature().typeID(); }

    FunctionSignature const &signature() const { return _sig; }

  private:
    FunctionSignature _sig;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_FUNCTION_H_
