#include "Volatile.h"

int main(int argc, char const* const* argv) {
    using namespace playground;
#if defined(__APPLE__)
    std::filesystem::path const filepath =
        std::filesystem::path(PROJECT_LOCATION) / "playground/Test.sc";
    volatilePlayground(filepath);
#else
    std::terminate();
#endif
}
