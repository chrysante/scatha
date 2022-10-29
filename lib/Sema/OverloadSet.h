#ifndef SCATHA_SEMA_OVERLOADSET_H_
#define SCATHA_SEMA_OVERLOADSET_H_

#include <span>

#include <utl/hashset.hpp>

#include "Sema/EntityBase.h"
#include "Sema/Function.h"
#include "Sema/SymbolID.h"

namespace scatha::sema {

class SCATHA(API) OverloadSet: public EntityBase {
public:
    explicit OverloadSet(std::string name, SymbolID id, Scope* parentScope):
        EntityBase(std::move(name), id, parentScope) {}

    /// Here overload resolution is happening
    Function const* find(std::span<TypeID const> argumentTypes) const;

    std::pair<Function*, bool> add(Function*);

    auto begin() const { return functions.begin(); }
    auto end() const { return functions.end(); }

private:
    utl::hashset<Function*, Function::ArgumentsHash, Function::ArgumentsEqual> functions;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_OVERLOADSET_H_
