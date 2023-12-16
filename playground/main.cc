#include "Volatile.h"

int main(int argc, char const* const* argv) {
    using namespace playground;
    std::filesystem::path const filepath =
        std::filesystem::path(PROJECT_LOCATION) / "playground/Test.sc";
    volatilePlayground(filepath);
}
