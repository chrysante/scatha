#ifndef SCATHA_OPT_PASS_H_
#define SCATHA_OPT_PASS_H_

#include <functional>
#include <string>

#include "IR/Fwd.h"

namespace scatha::opt {

/// Represents a local transform pass
class LocalPass {
public:
    using PointerType = bool (*)(ir::Context&, ir::Function&);

    LocalPass() = default;

    LocalPass(PointerType ptr): LocalPass(std::function(ptr)) {}

    LocalPass(std::function<bool(ir::Context&, ir::Function&)> p,
              std::string name = "anonymous"):
        p(std::move(p)), _name(std::move(name)) {}

    bool operator()(ir::Context& ctx, ir::Function& function) const {
        return p(ctx, function);
    }

    std::string const& name() const { return _name; }

    operator bool() const { return !!p; }

private:
    std::function<bool(ir::Context&, ir::Function&)> p;
    std::string _name;
};

/// Represents a global transform pass
class GlobalPass {
public:
    using PointerType = bool (*)(ir::Context&, ir::Module&, LocalPass);

    GlobalPass() = default;

    GlobalPass(PointerType ptr): GlobalPass(std::function(ptr)) {}

    GlobalPass(std::function<bool(ir::Context&, ir::Module&, LocalPass)> p,
               std::string name = "anonymous"):
        p(std::move(p)), _name(std::move(name)) {}

    bool operator()(ir::Context& ctx,
                    ir::Module& mod,
                    LocalPass localPass) const {
        return p(ctx, mod, std::move(localPass));
    }

    std::string const& name() const { return _name; }

    operator bool() const { return !!p; }

private:
    std::function<bool(ir::Context&, ir::Module&, LocalPass)> p;
    std::string _name;
};

} // namespace scatha::opt

#endif // SCATHA_OPT_PASS_H_
