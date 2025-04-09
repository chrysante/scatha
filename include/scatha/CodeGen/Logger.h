#ifndef SCATHA_CODEGEN_LOGGER_H_
#define SCATHA_CODEGEN_LOGGER_H_

#include <iosfwd>
#include <string_view>

#include <scatha/Common/Base.h>

namespace scatha::mir {

class Module;

} // namespace scatha::mir

namespace scatha::cg {

/// Logs the state of a module during code generation
class SCATHA_API Logger {
public:
    virtual ~Logger() = default;

    virtual void log(std::string_view stage, mir::Module const& mod) = 0;
};

/// Logs nothing
class SCATHA_API NullLogger: public Logger {
public:
    void log(std::string_view, mir::Module const&) override {}
};

/// Write verbose debug logs to `ostream`
class SCATHA_API DebugLogger: public Logger {
public:
    /// Initialize `ostream` with `std::cout`
    DebugLogger();

    explicit DebugLogger(std::ostream& ostream): ostream(ostream) {}

    void log(std::string_view stage, mir::Module const& mod) override;

private:
    std::ostream& ostream;
};

} // namespace scatha::cg

#endif // SCATHA_CODEGEN_LOGGER_H_
