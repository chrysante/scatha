// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_SEMA_OVERLOADSET_H_
#define SCATHA_SEMA_OVERLOADSET_H_

#include <span>

#include <utl/hashset.hpp>

#include <scatha/Sema/Entity.h>
#include <scatha/Sema/Function.h>
#include <scatha/Sema/SymbolID.h>

namespace scatha::sema {

class SCATHA_API OverloadSet: public Entity {
public:
    /// Construct an empty overload set.
    explicit OverloadSet(std::string name, SymbolID id, Scope* parentScope):
        Entity(EntityType::OverloadSet, std::move(name), id, parentScope) {}

    /// Resolve best matching function from this overload set for \p
    /// argumentTypes Returns `nullptr` if no matching function exists in the
    /// overload set.
    Function const* find(std::span<Type const* const> argumentTypes) const;

    /// \brief Add a function to this overload set.
    /// \returns Pair of \p function and `true` if \p function is a legal
    /// overload. Pair of pointer to existing function that prevents \p function
    /// from being a legal overload and `false` otherwise.
    std::pair<Function const*, bool> add(Function* function);

    /// Begin iterator to set of `Function`'s
    auto begin() const { return functions.begin(); }
    /// End iterator to set of `Function`'s
    auto end() const { return functions.end(); }

private:
    utl::small_vector<Function*, 8> functions;
    utl::hashset<Function*,
                 internal::FunctionArgumentsHash,
                 internal::FunctionArgumentsEqual>
        funcSet;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_OVERLOADSET_H_
