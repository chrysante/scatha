#ifndef SCATHA_PASSTOOL_COMMAND_H_
#define SCATHA_PASSTOOL_COMMAND_H_

#include <CLI/CLI11.hpp>

namespace scatha::passtool {

class Command {
public:
    explicit Command(CLI::App* parent, std::string name);

    explicit operator bool() const { return app()->parsed(); }

protected:
    CLI::App* app() { return _app; }
    CLI::App const* app() const { return _app; }

private:
    CLI::App* _app = nullptr;
};

} // namespace scatha::passtool

#endif // SCATHA_PASSTOOL_COMMAND_H_
