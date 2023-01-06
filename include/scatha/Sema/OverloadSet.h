// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_SEMA_OVERLOADSET_H_
#define SCATHA_SEMA_OVERLOADSET_H_

#include <span>

#include <utl/hashset.hpp>

#include <scatha/Sema/EntityBase.h>
#include <scatha/Sema/Function.h>
#include <scatha/Sema/SymbolID.h>

namespace scatha::sema {

class SCATHA(API) OverloadSet: public EntityBase {
public:
    /// Construct an empty overload set.
    explicit OverloadSet(std::string name, SymbolID id, Scope* parentScope):
        EntityBase(std::move(name), id, parentScope) {}

    /// Resolve best matching function from this overload set for \p argumentTypes
    /// Returns NULL if no matching function exists in the overload set.
    Function const* find(std::span<TypeID const> argumentTypes) const;

    /// \brief Add a function to this overload set.
    /// \returns Pair of \p function and \p true if \p function is a legal overload.
    /// Pair of pointer to existing function that prevents \p from being a legal overload and \p false otherwise.
    std::pair<Function const*, bool> add(Function* function);

    /// Begin iterator to set of \p Function 's
    auto begin() const { return functions.begin(); }
    /// End iterator to set of \p Function 's
    auto end() const { return functions.end(); }

private:
    utl::hashset<Function*, internal::FunctionArgumentsHash, internal::FunctionArgumentsEqual> functions;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_OVERLOADSET_H_
