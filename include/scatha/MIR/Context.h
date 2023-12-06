#ifndef SCATHA_MIR_CONTEXT_H_
#define SCATHA_MIR_CONTEXT_H_

#include <memory>

#include <utl/hashtable.hpp>

#include <scatha/Common/APInt.h>
#include <scatha/MIR/Fwd.h>

namespace scatha::mir {

/// Basically a constant pool for the MIR module
class SCATHA_API Context {
public:
    Context();
    Context(Context&&) noexcept;
    Context& operator=(Context&&) noexcept;
    ~Context();

    /// \Returns the global constant of byte width \p byteWidth with  value \p
    /// value
    Constant* constant(uint64_t value, size_t bytewidth);

    /// \overload
    Constant* constant(APInt value);

    /// \Return the global undef constant
    UndefValue* undef() const { return _undef.get(); }

private:
    utl::node_hashmap<std::pair<uint64_t, size_t>, Constant> constants;
    std::unique_ptr<UndefValue> _undef;
};

} // namespace scatha::mir

#endif // SCATHA_MIR_CONTEXT_H_
