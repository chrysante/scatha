// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_SEMA_FUNCTION_H_
#define SCATHA_SEMA_FUNCTION_H_

#include <scatha/Sema/Attributes.h>
#include <scatha/Sema/Entity.h>
#include <scatha/Sema/FunctionSignature.h>

namespace scatha::sema {

enum class AccessSpecifier { Public, Private };

class SCATHA_API Function: public Scope {
public:
    explicit Function(std::string name,
                      SymbolID functionID,
                      SymbolID overloadSetID,
                      Scope* parentScope,
                      FunctionAttribute attrs):
        Scope(EntityType::Function,
              ScopeKind::Function,
              std::move(name),
              functionID,
              parentScope),
        attrs(attrs),
        _overloadSetID(overloadSetID) {}

    /// \Returns The type ID of this function.
    Type const* type() const { return signature().type(); }

    /// \Returns The overload set ID of this function.
    SymbolID overloadSetID() const { return _overloadSetID; }

    /// \Returns The signature of this function.
    FunctionSignature const& signature() const { return _sig; }

    /// \Returns `true` iff this function is an external function.
    bool isExtern() const { return _isExtern; }

    /// \returns Slot of extern function table.
    ///
    /// Only applicable if this function is extern.
    size_t slot() const { return _slot; }

    /// \returns Index into slot of extern function table.
    ///
    /// Only applicable if this function is extern.
    size_t index() const { return _index; }

    /// \returns Bitfield of function attributes
    FunctionAttribute attributes() const { return attrs; }

    AccessSpecifier accessSpecifier() const { return accessSpec; }

    void setAccessSpecifier(AccessSpecifier spec) { accessSpec = spec; }

    /// Set attribute \p attr to `true`.
    void setAttribute(FunctionAttribute attr) { attrs |= attr; }

    /// Set attribute \p attr to `false`.
    void removeAttribute(FunctionAttribute attr) { attrs &= ~attr; }

private:
    friend class SymbolTable; /// To set `_sig` and `_isExtern`
    FunctionSignature _sig;
    SymbolID _overloadSetID;
    FunctionAttribute attrs;
    AccessSpecifier accessSpec = AccessSpecifier::Private;
    u32 _slot      : 31        = 0;
    bool _isExtern : 1         = false;
    u32 _index                 = 0;
};

namespace internal {

struct FunctionArgumentsHash {
    struct is_transparent;
    size_t operator()(Function const* f) const {
        return f->signature().argumentHash();
    }
    size_t operator()(std::span<QualType const* const> const& args) const {
        return FunctionSignature::hashArguments(args);
    }
};

struct FunctionArgumentsEqual {
    struct is_transparent;

    using Args = std::span<QualType const* const>;

    bool operator()(Function const* a, Function const* b) const {
        return a->signature().argumentHash() == b->signature().argumentHash();
    }
    bool operator()(Function const* a, Args b) const {
        return a->signature().argumentHash() ==
               FunctionSignature::hashArguments(b);
    }
    bool operator()(Args a, Function const* b) const { return (*this)(b, a); }
    bool operator()(Args a, Args b) const {
        return FunctionSignature::hashArguments(a) ==
               FunctionSignature::hashArguments(b);
    }
};

} // namespace internal

} // namespace scatha::sema

#endif // SCATHA_SEMA_FUNCTION_H_
