#ifndef SCATHA_SEMA_NAMEMANGLING_H_
#define SCATHA_SEMA_NAMEMANGLING_H_

#include <optional>
#include <string>

#include <scatha/Sema/Fwd.h>

namespace scatha::sema {

/// Options structure for name mangling
struct NameManglingOptions {
    /// A global name prefix to be added to all entities. This is used to
    /// disambiguate names in libraries
    std::optional<std::string> globalPrefix;
};

/// Name mangling function object that takes a set of options
class NameMangler {
public:
    /// Construct with default options
    NameMangler() = default;

    /// Construct from a set of options
    explicit NameMangler(NameManglingOptions options):
        _options(std::move(options)) {}

    /// \Returns the mangled name of \p entity
    std::string operator()(Entity const& entity) const;

    /// \Returns the options
    NameManglingOptions const& options() const { return _options; }

private:
    NameManglingOptions _options = {};
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_NAMEMANGLING_H_
