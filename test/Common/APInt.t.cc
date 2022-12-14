#include <Catch/Catch2.hpp>

#include "Common/APInt.h"

#include <sstream>

using namespace scatha;

TEST_CASE("APInt comparison", "[common][big-num]") {
    APInt n = 300;
    CHECK(n == APInt(300));
    CHECK(n > APInt(0));
    CHECK(n < APInt(1000));
    CHECK(n == 300);
    CHECK(n > 0);
    CHECK(n < 1000);
    CHECK(300 == n);
    CHECK(0 < n);
    CHECK(1000 > n);
}

TEST_CASE("APInt representable - 1", "[common][big-num]") {
    APInt n = 300;
    CHECK(n.representableAs<int>());
    CHECK(n.representableAs<unsigned int>());
    CHECK(n.representableAs<signed long>());
    CHECK(n.representableAs<unsigned long>());
    CHECK(!n.representableAs<char>());
    CHECK(!n.representableAs<unsigned char>());
    CHECK(n.representableAs<float>());
    CHECK(n.representableAs<double>());
    CHECK(n.representableAs<long double>());
}

TEST_CASE("APInt representable - 2", "[common][big-num]") {
    APInt n = APInt::fromString("FFffFFffFFffFFffFFffFFffFFffFFff", 16).value();
    CHECK(!n.representableAs<int>());
    CHECK(!n.representableAs<unsigned int>());
    CHECK(!n.representableAs<signed long>());
    CHECK(!n.representableAs<unsigned long>());
    CHECK(!n.representableAs<char>());
    CHECK(!n.representableAs<unsigned char>());
}

TEST_CASE("APInt representable - 3", "[common][big-num]") {
    APInt n = -200;
    CHECK(n.representableAs<int>());
    CHECK(!n.representableAs<unsigned int>());
    CHECK(n.representableAs<signed long>());
    CHECK(!n.representableAs<unsigned long>());
    CHECK(!n.representableAs<char>());
    CHECK(!n.representableAs<unsigned char>());
    CHECK(n.representableAs<float>());
    CHECK(n.representableAs<double>());
    CHECK(n.representableAs<long double>());
    CHECK(static_cast<int>(n) == -200);
}

TEST_CASE("APInt fromString()", "[common][big-num]") {
    CHECK(APInt::fromString("123").value() == 123);
    CHECK(APInt::fromString("123", 16).value() == 0x123);
}

TEST_CASE("APInt arithmetic", "[common][big-num]") {
    SECTION("Addition") {
        APInt n = 100;
        n += 1;
        CHECK(n == 101);
    }
    SECTION("Addition overflow") {
        APInt n = APInt(0xFFffFFffFFffFFff);
        n += 1;
        CHECK(n == APInt::fromString("0x10000000000000000").value());
    }
    SECTION("Subtraction") {
        APInt n = 100;
        n -= 1;
        CHECK(n == 99);
    }
    SECTION("Subtraction overflow") {
        APInt n = APInt::fromString("0x10000000000000000").value();
        n -= APInt(1);
        CHECK(n == 0xFFffFFffFFffFFff);
    }
    SECTION("Multiplication") {
        APInt n = 2;
        n *= 7;
        CHECK(n == 14);
    }
    SECTION("Division") {
        APInt n = 1;
        n /= 2;
        CHECK(n == 0);
    }
}

TEST_CASE("APInt formatting", "[common][big-num]") {
    std::stringstream sstr;
    SECTION("Positive") {
        APInt const n = 100;
        sstr << n;
        CHECK(sstr.str() == "100");
    }
    SECTION("Negative") {
        APInt const n = -100;
        sstr << n;
        CHECK(sstr.str() == "-100");
    }
}
