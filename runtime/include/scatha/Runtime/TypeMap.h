#ifndef SCATHA_RUNTIME_TYPEMAP_H_
#define SCATHA_RUNTIME_TYPEMAP_H_

#include <typeindex>
#include <unordered_map>

#include <scatha/Sema/Fwd.h>

namespace scatha {

/// Maps C++ types to Scatha types
class TypeMap {
public:
    /// Associate C++ type \p key to the Scatha type \p value
    sema::Type const* insert(std::type_info const& key,
                             sema::Type const* value);

private:
    std::unordered_map<std::type_index, sema::Type const*> typeMap;
};

} // namespace scatha

#endif // SCATHA_RUNTIME_TYPEMAP_H_
