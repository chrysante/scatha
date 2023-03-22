#include <Catch/Catch2.hpp>

#include "IR/UniqueName.h"

using namespace scatha;
using namespace ir;

TEST_CASE("UniqueNameFactory", "[ir]") {
    UniqueNameFactory f;

    CHECK(f.makeUnique("a") == "a");
    CHECK(f.makeUnique("a") == "a.0");
    CHECK(f.makeUnique("a") == "a.1");
    CHECK(f.makeUnique("a.2") == "a.2");
    CHECK(f.makeUnique("a") == "a.3");
    f.erase("a.2");
    CHECK(f.makeUnique("a") == "a.2");

    CHECK(f.makeUnique("") == "");
    CHECK(f.makeUnique(".") == ".");
    CHECK(f.makeUnique(".") == "..0");

    CHECK(f.makeUnique("b.100") == "b.100");
    CHECK(f.makeUnique("b.100") == "b.101");

    CHECK(f.makeUnique("a.b.c.99.100") == "a.b.c.99.100");
    CHECK(f.makeUnique("a.b.c.99.100") == "a.b.c.99.101");
}
