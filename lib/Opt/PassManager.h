#ifndef SCATHA_OPT_PASSMANAGER_H_
#define SCATHA_OPT_PASSMANAGER_H_

#include <functional>
#include <string>
#include <string_view>

#include <utl/vector.hpp>

#include "Common/Base.h"
#include "IR/Fwd.h"

namespace scatha::opt {

class SCATHA_API PassManager {
public:
    using PassType = bool (*)(ir::Context&, ir::Function&);

    static std::function<bool(ir::Context&, ir::Function&)> getPass(
        std::string_view name);

    static std::function<bool(ir::Context&, ir::Module&)> makePipeline(
        std::string_view passes);

    static utl::vector<std::string> passes();
};

} // namespace scatha::opt

#define SC_REGISTER_PASS(function, name)                                       \
    static int SC_CONCAT(__Pass_, __COUNTER__) = [] {                          \
        ::scatha::opt::internal::registerPass(function, name);                 \
        return 0;                                                              \
    }()

namespace scatha::opt::internal {

void registerPass(PassManager::PassType pass, std::string name);

} // namespace scatha::opt::internal

#endif // SCATHA_OPT_PASSMANAGER_H_
