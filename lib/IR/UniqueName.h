#ifndef SCATHA_IR_UNIQUENAME_H_
#define SCATHA_IR_UNIQUENAME_H_

#include <string>

#include <utl/hashtable.hpp>

#include "Basic/Basic.h"

namespace scatha::ir {

class SCATHA(TEST_API) UniqueNameFactory {
public:
    /// Make a uniqued variation of \p name
    ///
    /// \returns Uniqued name.
    ///
    /// \details Also registers this name to make sure it stays unique.
    std::string makeUnique(std::string name);

    /// Try to register \p name
    ///
    /// \returns `true` if \p name was registered successfully.
    ///
    /// \details This function is not needed when `makeUnique()` is used. It is
    /// only needed to register a name without potentially changing it.
    bool tryRegister(std::string_view name);

    /// Erase \p name
    /// This makes the name available again.
    ///
    /// \pre \p name must be registered.
    void erase(std::string_view name);

    /// Erase \p name if it exists.
    /// This makes the name available again.
    void tryErase(std::string_view name);

private:
    std::string appendCounter(std::string name, size_t start);

private:
    utl::hashset<std::string> _knownNames;
};

} // namespace scatha::ir

#endif // SCATHA_IR_UNIQUENAME_H_
