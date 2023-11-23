#ifndef SCATHA_IR_PASS_H_
#define SCATHA_IR_PASS_H_

#include <functional>
#include <string>

#include <scatha/IR/Fwd.h>

namespace scatha::ir {

/// Represents a local transform pass that operates on a single function
class LocalPass {
public:
    /// The function pointer type with the signature of the pass type
    using PointerType = bool (*)(ir::Context&, ir::Function&);

    /// Construct an empty local pass. Empty passes are invalid an can not be
    /// executed.
    LocalPass() = default;

    /// Construct a local pass from function pointer \p pointer
    LocalPass(PointerType ptr): LocalPass(std::function(ptr)) {}

    /// Construct a named local pass from a function
    LocalPass(std::function<bool(ir::Context&, ir::Function&)> p,
              std::string name = "anonymous"):
        p(std::move(p)), _name(std::move(name)) {}

    /// Invoke the pass
    bool operator()(ir::Context& ctx, ir::Function& function) const {
        return p(ctx, function);
    }

    /// The name of the pass
    std::string const& name() const { return _name; }

    /// \Returns `true` is the pass is non-empty
    operator bool() const { return !!p; }

private:
    std::function<bool(ir::Context&, ir::Function&)> p;
    std::string _name;
};

/// Represents a global transform pass that operates on an entire module
class GlobalPass {
public:
    /// The function pointer type with the signature of the pass type
    using PointerType = bool (*)(ir::Context&, ir::Module&, LocalPass);

    /// Construct an empty global pass. Empty passes are invalid an can not be
    /// executed.
    GlobalPass() = default;

    /// Construct a global pass from function pointer \p pointer
    GlobalPass(PointerType ptr): GlobalPass(std::function(ptr)) {}

    /// Construct a named global pass from a function
    GlobalPass(std::function<bool(ir::Context&, ir::Module&, LocalPass)> p,
               std::string name = "anonymous"):
        p(std::move(p)), _name(std::move(name)) {}

    /// Invoke the pass
    bool operator()(ir::Context& ctx,
                    ir::Module& mod,
                    LocalPass localPass) const {
        return p(ctx, mod, std::move(localPass));
    }

    /// The name of the pass
    std::string const& name() const { return _name; }

    /// \Returns `true` is the pass is non-empty
    operator bool() const { return !!p; }

private:
    std::function<bool(ir::Context&, ir::Module&, LocalPass)> p;
    std::string _name;
};

} // namespace scatha::ir

#endif // SCATHA_IR_PASS_H_
