#ifndef SCATHA_IR_PASS_H_
#define SCATHA_IR_PASS_H_

#include <functional>
#include <string>

#include <scatha/IR/Fwd.h>

namespace scatha::ir {

/// Different pass categories
enum class PassCategory {
    ///
    Analysis,

    /// Canonicalization passes bring the IR in canonical form
    Canonicalization,

    ///
    Simplification,

    ///
    Optimization,

    /// We put experimental passes here so we can access them through the pass
    /// manager but we can ignore them in the automatic pass tests
    Experimental,

    /// For now here we have 'print' and 'foreach'
    Other
};

/// Common base class of `LocalPass` and `GlobalPass`
class PassBase {
public:
    /// The name of the pass
    std::string const& name() const { return _name; }

    /// The category of this pass
    PassCategory category() const { return _cat; }

protected:
    PassBase(): PassBase({}, PassCategory::Other) {}

    PassBase(std::string name, PassCategory category):
        _name(std::move(name)), _cat(category) {
        if (_name.empty()) {
            _name = "anonymous";
        }
    }

private:
    std::string _name;
    PassCategory _cat;
};

/// Represents a local pass that operates on a single function
class LocalPass: public PassBase {
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
              std::string name = {},
              PassCategory category = PassCategory::Other):
        PassBase(std::move(name), category), p(std::move(p)) {}

    /// Invoke the pass
    bool operator()(ir::Context& ctx, ir::Function& function) const {
        return p(ctx, function);
    }

    /// \Returns `true` is the pass is non-empty
    operator bool() const { return !!p; }

private:
    std::function<bool(ir::Context&, ir::Function&)> p;
};

/// Represents a global pass that operates on an entire module
class GlobalPass: public PassBase {
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
               std::string name = {},
               PassCategory category = PassCategory::Other):
        PassBase(std::move(name), category), p(std::move(p)) {}

    /// Invoke the pass
    bool operator()(ir::Context& ctx,
                    ir::Module& mod,
                    LocalPass localPass) const {
        return p(ctx, mod, std::move(localPass));
    }

    /// \Returns `true` is the pass is non-empty
    operator bool() const { return !!p; }

private:
    std::function<bool(ir::Context&, ir::Module&, LocalPass)> p;
};

} // namespace scatha::ir

#endif // SCATHA_IR_PASS_H_
